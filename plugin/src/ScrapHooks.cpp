#include "PCH.h"

#include "ScrapHooks.h"

#include "Config.h"
#include "MenuAccess.h"
#include "logic/DialogLabel.h"
#include "logic/PendingScrap.h"
#include "logic/ScrapYield.h"
#include "logic/Selection.h"

#include <windows.h>

// wingdi.h defines ERROR as a macro, which collides with REX::ERROR.
#undef ERROR

namespace ScrapStacks::ScrapHooks
{
	namespace
	{
		constexpr std::size_t kOnAcceptSlot{ 1 };

		// The accept callback vanilla allocates in the ScrapItem case (0x18 bytes).
		struct ScrapItemCallback
		{
			void*             vtbl;
			RE::ExamineMenu*  menu;
			std::uint32_t     rowIndex;
		};

		using Yield = std::vector<YieldEntry<RE::TESBoundObject*>>;

		PendingScrap  g_pending;
		StackDecision g_decision;  // what this Scrap press became, for the dialog label

		// Copies still to remove once vanilla has removed its one, handed from the
		// accept hook to the after-removal hook inside the same ScrapItemAccepted call.
		struct RemainingCopies
		{
			MenuAccess::Row row;
			std::uint32_t   count{ 0 };
		};
		std::optional<RemainingCopies> g_remaining;

		bool Diag() { return Config::Get().diagnostics; }

		bool ModifierHeld()
		{
			return (GetAsyncKeyState(Config::Get().stackModifierKey) & 0x8000) != 0;
		}

		Yield CurrentYield(RE::ExamineMenu* a_menu)
		{
			Yield yield;
			for (const auto& entry : MenuAccess::ScrappingArray(a_menu)) {
				yield.push_back({ entry.first, entry.second });
			}
			return yield;
		}

		std::string Describe(const Yield& a_yield)
		{
			std::string text;
			for (const auto& entry : a_yield) {
				const char* name = entry.item ? RE::TESFullName::GetFullName(*entry.item).data() : "?";
				text += std::format("{}{} x{}", text.empty() ? "" : ", ", name, entry.count);
			}
			return text.empty() ? "(nothing)" : text;
		}

		// Discovery already checked each call site and its target; confirm it is still a
		// plain call at the moment we patch it.
		bool IsCall(std::uintptr_t a_site)
		{
			return *reinterpret_cast<const std::uint8_t*>(a_site) == 0xE8;
		}

		// ── Hook 1: yield ────────────────────────────────────────────────────
		// Replaces the ScrapItem case's call to BuildWeaponScrappingArray. With the
		// modifier held on a row of several identical copies, the vanilla calculation
		// runs once per copy and the per-copy results are summed into the array the
		// confirm dialog and the grant both read.
		struct BuildYield
		{
			static void thunk(RE::ExamineMenu* a_menu)
			{
				g_pending.Disarm();
				g_decision = {};
				func(a_menu);

				if (!ModifierHeld()) {
					return;
				}

				const auto rowIndex = static_cast<std::int32_t>(a_menu->GetSelectedIndex());
				const auto row = MenuAccess::ReadRow(a_menu, rowIndex);
				if (!row) {
					REX::WARN("stack scrap: could not read row {}, scrapping one copy", rowIndex);
					return;
				}
				if (row->hasFavorite) {
					REX::INFO("stack scrap: row {} has a favorited copy, scrapping one copy", rowIndex);
					g_decision = { StackDecision::Kind::kFavorite };
					return;
				}
				if (!VanillaPickMatches(row->stacks)) {
					// Vanilla would remove a copy whose mods differ from the one the
					// yield is computed from; leave that case entirely to vanilla.
					REX::WARN("stack scrap: row {}: the copy the game removes first has different mods ({}) from the one it shows ({}), scrapping one copy",
						rowIndex, row->otherMods, row->scrappedMods);
					g_decision = { StackDecision::Kind::kMixedMods };
					return;
				}
				const auto copies = ScrappableCount(row->stacks);
				if (copies < 2) {
					REX::INFO("stack scrap: row {} has {} matching unequipped cop{}, scrapping normally", rowIndex, copies, copies == 1 ? "y" : "ies");
					if (!row->otherMods.empty()) {
						g_decision = { StackDecision::Kind::kMixedMods };
					}
					return;
				}
				if (!row->otherMods.empty()) {
					REX::INFO("stack scrap: row {} also holds copies with other mods {}; scrapping only the {} with {}",
						rowIndex, row->otherMods, copies, row->scrappedMods);
				}

				const auto single = CurrentYield(a_menu);
				Yield total;
				AddYield(total, single);
				for (std::uint32_t i = 1; i < copies; ++i) {
					func(a_menu);
					AddYield(total, CurrentYield(a_menu));
				}

				auto& array = MenuAccess::ScrappingArray(a_menu);
				array.clear();
				for (const auto& entry : total) {
					array.push_back({ entry.item, entry.count });
				}
				g_pending.Arm(static_cast<std::uint32_t>(rowIndex), copies);
				std::uint32_t rowCopies = 0;
				for (const auto& stack : row->stacks) {
					rowCopies += stack.equipped ? 0 : stack.count;
				}
				g_decision = { StackDecision::Kind::kStack, copies, rowCopies };

				REX::INFO("stack scrap armed: row {}, {} copies of {}", rowIndex, copies, RE::TESFullName::GetFullName(*row->object));
				if (Diag()) {
					REX::INFO("  one copy yields: {}", Describe(single));
					REX::INFO("  per-copy sum:    {}", Describe(total));
					for (const auto& entry : total) {
						const auto it = std::ranges::find(single, entry.item, &YieldEntry<RE::TESBoundObject*>::item);
						const auto expected = it == single.end() ? 0 : it->count * copies;
						if (entry.count != expected) {
							REX::WARN("  per-copy sum differs from {} x one copy for this component: {} vs {}", copies, entry.count, expected);
						}
					}
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// ── Hook 2: dialog label ─────────────────────────────────────────────
		// Says in the confirm dialog what a Shift+Scrap became: "Name (x5)", "(x5 of 8,
		// rest have other mods)", or why it fell back to one copy. The name is copied
		// into a BSFixedStringCS, so our buffer only needs to outlive the call.
		struct InitDataScrap
		{
			static void* thunk(void* a_this, const char* a_question, const char* a_label, const char* a_name, void* a_results)
			{
				std::string label;
				if (const auto suffix = LabelSuffix(g_decision); !suffix.empty()) {
					label = std::string{ a_name ? a_name : "" } + suffix;
					a_name = label.c_str();
				}
				return func(a_this, a_question, a_label, a_name, a_results);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// ── Hook 3: accept ───────────────────────────────────────────────────
		// Vanilla grants the (summed) yield and removes one copy. For a stack scrap
		// the remaining copies are then removed from the same stacks, found again by
		// address because removal can shift stack indices.
		struct OnAccept
		{
			static void thunk(ScrapItemCallback* a_callback)
			{
				auto* const menu = a_callback->menu;
				const auto  row = static_cast<std::int32_t>(a_callback->rowIndex);
				const auto  copies = g_pending.Take(a_callback->rowIndex);
				const auto  before = copies ? MenuAccess::ReadRow(menu, row) : std::nullopt;
				const auto  screenRow = MenuAccess::DisplayRow(menu);
				const auto  scroll = MenuAccess::ScrollPosition(menu);
				if (Diag()) {
					REX::INFO("scrap accepted: engine row {}, screen row {}, {} rows", row, screenRow.value_or(-1), MenuAccess::DisplayRowCount(menu));
				}

				if (copies && before) {
					g_remaining = RemainingCopies{ *before, *copies - 1 };
				} else if (copies) {
					REX::ERROR("stack scrap: row {} could not be read at accept; only one copy will be removed but the summed yield is granted", row);
				}

				func(a_callback);

				if (g_remaining) {
					REX::ERROR("stack scrap: vanilla accept never reached the removal point; only one copy was removed but the summed yield was granted");
					g_remaining.reset();
				}
				KeepScreenRow(menu, screenRow, scroll);
			}

			// Vanilla's rebuild re-selects by engine index, which FallUI's sorted
			// view shows somewhere else once the scrapped row is gone. Put the cursor
			// back on the same place on screen, clamped to the shorter list.
			static void KeepScreenRow(RE::ExamineMenu* a_menu, std::optional<std::int32_t> a_screenRow, std::optional<std::int32_t> a_scroll)
			{
				if (!a_screenRow) {
					REX::WARN("keep selection: could not read the on-screen row before the scrap; leaving the selection as the game set it");
					return;
				}
				const auto landed = MenuAccess::DisplayRow(a_menu);
				const auto target = RowAfterScrap(*a_screenRow, MenuAccess::DisplayRowCount(a_menu));
				if (target < 0) {
					return;  // list is empty
				}
				const auto selected = MenuAccess::SelectDisplayRow(a_menu, target);
				if (selected && a_scroll) {
					MenuAccess::RestoreScrollPosition(a_menu, *a_scroll);
				}
				if (!selected) {
					REX::WARN("keep selection: could not select screen row {}", target);
				} else if (Diag()) {
					REX::INFO("keep selection: game left screen row {}, restored {} (asked {}), scroll {} -> {}", landed.value_or(-1), *selected, target, a_scroll.value_or(-1), MenuAccess::ScrollPosition(a_menu).value_or(-1));
				}
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		// ── Hook 4: remove the rest ──────────────────────────────────────────
		// Inside ScrapItemAccepted, between vanilla's single RemoveItem and its list
		// rebuild. Removing the other copies here means the one rebuild already sees
		// the final inventory. A second rebuild would drop FallUI's column data: FallUI
		// rescans item stats only on the first list update after Scrap is pressed.
		struct AfterVanillaRemoval
		{
			static void thunk(RE::WorkbenchMenuBase* a_menu)
			{
				if (g_remaining) {
					const auto remaining = std::move(*g_remaining);
					g_remaining.reset();
					RemoveRemaining(remaining.row, remaining.count);
				}
				func(a_menu);
			}

			static void RemoveRemaining(const MenuAccess::Row& a_before, std::uint32_t a_toRemove)
			{
				// Fresh index and count for each stack that still exists.
				std::vector<RowStack> current;
				for (const auto& stack : a_before.stacks) {
					if (const auto index = MenuAccess::FindStackIndex(a_before.object, stack.identity)) {
						auto fresh = stack;
						fresh.index = *index;
						fresh.count = reinterpret_cast<const RE::BGSInventoryItem::Stack*>(stack.identity)->GetCount();
						current.push_back(fresh);
					}
				}

				std::uint32_t removed = 0;
				for (const auto& removal : PlanRemovals(current, a_toRemove)) {
					const auto index = MenuAccess::FindStackIndex(a_before.object, removal.identity);
					if (!index) {
						REX::WARN("stack scrap: a stack vanished mid-removal");
						continue;
					}
					MenuAccess::RemoveFromStack(a_before.object, *index, removal.count);
					removed += removal.count;
				}

				if (removed == a_toRemove) {
					REX::INFO("stack scrap done: removed {} copies in total", removed + 1);
				} else {
					REX::ERROR("stack scrap: removed {} of {} remaining copies; the yield for all copies was granted", removed, a_toRemove);
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		template <class Hook>
		bool InstallCall(std::string_view a_name, std::uintptr_t a_site)
		{
			if (!IsCall(a_site)) {
				REX::ERROR("hook {}: code at {:#x} changed since discovery; hook NOT installed", a_name, a_site);
				return false;
			}
			Hook::func = REL::GetTrampoline().write_call<5>(a_site, Hook::thunk);
			REX::INFO("hook {}: installed", a_name);
			return true;
		}

		template <class Hook>
		void InstallVfunc(std::string_view a_name, std::uintptr_t a_vtable, std::size_t a_slot)
		{
			REL::Relocation<std::uintptr_t> vtbl{ a_vtable };
			// Chain whatever is there now, so another plugin that swapped this slot
			// first (e.g. Better Weapon Scrapping) keeps working.
			Hook::func = vtbl.write_vfunc(a_slot, Hook::thunk);
			REX::INFO("hook {}: installed", a_name);
		}
	}

	void Install(const HookSites& a_sites, std::uintptr_t a_base)
	{
		// Accept also keeps the on-screen row, so it goes in regardless.
		InstallVfunc<OnAccept>("accept", a_base + a_sites.scrapCallbackVtbl, kOnAcceptSlot);

		// A stack scrap grants every copy's yield, so it must only ever be armed when
		// the hook that removes the other copies is in place.
		if (!InstallCall<AfterVanillaRemoval>("remove the rest", a_base + a_sites.removalCall)) {
			REX::ERROR("stack scrap disabled: the copy-removal hook is missing");
			return;
		}
		InstallCall<BuildYield>("yield", a_base + a_sites.yieldCall);
		InstallCall<InitDataScrap>("dialog label", a_base + a_sites.labelCall);
	}
}
