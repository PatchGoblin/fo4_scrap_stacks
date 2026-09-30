#include "PCH.h"

#include "MenuAccess.h"

#include "logic/ModSignature.h"


namespace ScrapStacks::MenuAccess
{
	namespace
	{
		// Layout verified against 1.10.163 (docs/re-findings.md). The compiler checks
		// CommonLib's declared offsets against what the executable was seen to use.
		static_assert(offsetof(RE::ExamineMenu, invInterface) == 0x370);
		static_assert(offsetof(RE::InventoryUserUIInterface, stackedEntries) == 0x60);
		static_assert(offsetof(RE::InventoryUserUIInterface, entriesInvalid) == 0x78);
		static_assert(offsetof(RE::ExamineMenu, scrappingArray) == 0x428);
		static_assert(offsetof(RE::ExamineMenu, itemList) == 0x488);
		static_assert(offsetof(RE::BGSInventoryItem::Stack, nextStack) == 0x10);
		static_assert(offsetof(RE::BGSInventoryItem::Stack, count) == 0x20);

		// CommonLib declares InventoryUserUIInterfaceEntry::stackIndex with 8-bit
		// elements, but the executable reads 16-bit ones. This view matches the code.
		struct RowEntryView
		{
			std::uint32_t handle;         // 00
			std::uint32_t pad04;          // 04
			std::uint32_t capacityFlags;  // 08  bit 31 set = elements stored inline
			std::uint32_t pad0C;          // 0C
			union
			{
				std::uint16_t  inlineData[4];
				std::uint16_t* heapData;
			};                   // 10
			std::uint32_t size;  // 18
			std::uint32_t pad1C;  // 1C
		};
		static_assert(sizeof(RowEntryView) == 0x20);
		static_assert(sizeof(RE::InventoryUserUIInterfaceEntry) == 0x20);

		std::span<const std::uint16_t> StackIndices(const RowEntryView& a_entry)
		{
			const auto* data = (a_entry.capacityFlags & 0x80000000u) != 0 ? a_entry.inlineData : a_entry.heapData;
			return { data, a_entry.size };
		}

		const RowEntryView* GetRowEntry(RE::ExamineMenu* a_menu, std::int32_t a_rowIndex)
		{
			auto& ui = a_menu->invInterface;
			if (ui.entriesInvalid || a_rowIndex < 0 ||
				static_cast<std::uint32_t>(a_rowIndex) >= ui.stackedEntries.size()) {
				return nullptr;
			}
			return reinterpret_cast<const RowEntryView*>(std::addressof(ui.stackedEntries[a_rowIndex]));
		}

		std::vector<ModEntry> ToEntries(std::span<const RE::BGSMod::ObjectIndexData> a_data)
		{
			std::vector<ModEntry> mods;
			for (const auto& data : a_data) {
				mods.push_back({ data.objectID, data.index, data.rank, data.disabled });
			}
			return mods;
		}

		std::vector<ModEntry> ModsOf(const RE::BGSInventoryItem::Stack& a_stack)
		{
			if (!a_stack.extra) {
				return {};
			}
			const auto* instance = a_stack.extra->GetByType<RE::BGSObjectInstanceExtra>();
			return instance ? ToEntries(instance->GetIndexData()) : std::vector<ModEntry>{};
		}

		// Mod list with in-game names, for the log.
		std::string DescribeWithNames(const std::vector<ModEntry>& a_mods)
		{
			std::string text = Describe(a_mods);
			std::string names;
			for (const auto& mod : a_mods) {
				const auto* form = RE::TESForm::GetFormByID(mod.formID);
				const auto  name = form ? RE::TESFullName::GetFullName(*form) : std::string_view{};
				names += std::format("{}{}", names.empty() ? "" : ", ", name.empty() ? "?" : name);
			}
			return a_mods.empty() ? text : std::format("{} ({})", text, names);
		}

		// Engine virtuals are called by the slot numbers seen in the executable
		// (docs/re-findings.md), not by CommonLib's declaration order.
		template <class R = void, class... Args>
		R CallVfunc(void* a_object, std::size_t a_slot, Args... a_args)
		{
			const auto* vtbl = *static_cast<std::uintptr_t* const*>(a_object);
			return reinterpret_cast<R (*)(void*, Args...)>(vtbl[a_slot])(a_object, a_args...);
		}

		namespace Slot
		{
			constexpr std::size_t kGetObjectInstanceExtra{ 0x26 };  // ExamineMenu
			constexpr std::size_t kRemoveItem{ 0x6D };              // TESObjectREFR
		}

		RE::BGSInventoryItem* FindPlayerItem(RE::TESBoundObject* a_object)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->inventoryList) {
				return nullptr;
			}
			for (auto& item : player->inventoryList->data) {
				if (item.object == a_object) {
					return std::addressof(item);
				}
			}
			return nullptr;
		}
	}

	std::uint32_t RowCount(RE::ExamineMenu* a_menu)
	{
		return a_menu->invInterface.entriesInvalid ? 0 : a_menu->invInterface.stackedEntries.size();
	}

	std::optional<Row> ReadRow(RE::ExamineMenu* a_menu, std::int32_t a_rowIndex)
	{
		const auto* entry = GetRowEntry(a_menu, a_rowIndex);
		if (!entry) {
			return std::nullopt;
		}
		const auto* item = RE::BGSInventoryInterface::GetSingleton()->RequestInventoryItem(entry->handle);
		if (!item || !item->object) {
			return std::nullopt;
		}

		const auto indices = StackIndices(*entry);
		Row row{ .object = item->object };

		// The copy the yield is computed from: BuildWeaponScrappingArray asks the
		// menu for exactly this (vfunc 0x26, docs/re-findings.md).
		const auto* reference = CallVfunc<const RE::BGSObjectInstanceExtra*>(a_menu, Slot::kGetObjectInstanceExtra);
		const auto  scrappedMods = reference ? ToEntries(reference->GetIndexData()) : std::vector<ModEntry>{};
		row.scrappedMods = DescribeWithNames(scrappedMods);

		std::uint32_t index = 0;
		for (auto* stack = item->stackData.get(); stack; stack = stack->nextStack.get(), ++index) {
			if (std::ranges::find(indices, static_cast<std::uint16_t>(index)) == indices.end()) {
				continue;
			}
			if (stack->extra && stack->extra->GetByType(RE::EXTRA_DATA_TYPE::kFavorite)) {
				row.hasFavorite = true;
			}
			const auto mods = ModsOf(*stack);
			const bool same = SameMods(mods, scrappedMods);
			row.stacks.push_back({
				.identity = reinterpret_cast<std::uintptr_t>(stack),
				.index = index,
				.count = stack->GetCount(),
				.equipped = stack->IsEquipped(),
				.sameModsAsScrapped = same,
			});
			if (!same && row.otherMods.empty()) {
				row.otherMods = DescribeWithNames(mods);
			}
		}
		return row;
	}

	RE::BSTArray<RE::BSTTuple<RE::TESBoundObject*, std::uint32_t>>& ScrappingArray(RE::ExamineMenu* a_menu)
	{
		return a_menu->scrappingArray;
	}

	namespace
	{
		// The scrolling list the player sees. menu->itemList is the AS3 ListInfoObject:
		// FallUI's exposes the list as .list, vanilla's keeps it private, so fall back
		// to where both menus put it in inventory mode (the only mode Scrap runs in).
		std::optional<Scaleform::GFx::Value> ScrollList(RE::ExamineMenu* a_menu)
		{
			Scaleform::GFx::Value list;
			if (a_menu->itemList.IsObject() && a_menu->itemList.GetMember("list", &list) && list.IsObject()) {
				return list;
			}
			if (a_menu->uiMovie && a_menu->uiMovie->GetVariable(&list, "root.BaseInstance.InventoryBase_mc.InventoryList_mc") && list.IsObject()) {
				return list;
			}
			return std::nullopt;
		}

		// Scaleform hands integers back as kInt, kUInt or kNumber depending on source.
		std::optional<std::int32_t> AsInt(const Scaleform::GFx::Value& a_value)
		{
			if (a_value.IsInt()) {
				return a_value.GetInt();
			}
			if (a_value.IsUInt()) {
				return static_cast<std::int32_t>(a_value.GetUInt());
			}
			if (a_value.IsNumber()) {
				return static_cast<std::int32_t>(a_value.GetNumber());
			}
			return std::nullopt;
		}

		// Where the on-screen row can be read and written. FallUI keeps it on the
		// list's BSListMod helper (list.mod.selectedIndexModNoShift), not on the list.
		// Vanilla has no helper, and there the plain selectedIndex already is the
		// on-screen row. If the helper exists but the member can't be read, the index
		// spaces are unknown, so give up rather than write an engine index as a
		// screen row.
		struct DisplayIndex
		{
			Scaleform::GFx::Value owner;
			const char*           member;
		};

		std::optional<DisplayIndex> ResolveDisplayIndex(const Scaleform::GFx::Value& a_list)
		{
			Scaleform::GFx::Value mod;
			if (!a_list.GetMember("mod", &mod) || !mod.IsObject()) {
				return DisplayIndex{ a_list, "selectedIndex" };
			}
			Scaleform::GFx::Value probe;
			if (mod.GetMember("selectedIndexModNoShift", &probe)) {
				return DisplayIndex{ mod, "selectedIndexModNoShift" };
			}
			REX::WARN("keep selection: FallUI's list helper has no selectedIndexModNoShift (FallUI update?); not touching the selection");
			return std::nullopt;
		}
	}

	std::optional<std::int32_t> DisplayRow(RE::ExamineMenu* a_menu)
	{
		const auto list = ScrollList(a_menu);
		const auto index = list ? ResolveDisplayIndex(*list) : std::nullopt;
		if (!index) {
			return std::nullopt;
		}
		Scaleform::GFx::Value row;
		if (!index->owner.GetMember(index->member, &row)) {
			return std::nullopt;
		}
		return AsInt(row);
	}

	std::uint32_t DisplayRowCount(RE::ExamineMenu* a_menu)
	{
		const auto list = ScrollList(a_menu);
		Scaleform::GFx::Value entries;
		if (!list || !list->GetMember("entryList", &entries) || !entries.IsArray()) {
			return 0;
		}
		return entries.GetArraySize();
	}

	std::optional<std::int32_t> SelectDisplayRow(RE::ExamineMenu* a_menu, std::int32_t a_row)
	{
		const auto list = ScrollList(a_menu);
		auto       index = list ? ResolveDisplayIndex(*list) : std::nullopt;
		if (!index) {
			return std::nullopt;
		}

		// Step off rows the current category filter hides, the way FallUI does itself.
		// The filter works on the list's entry array, which is in on-screen order.
		Scaleform::GFx::Value filterer;
		if (list->GetMember("filterer", &filterer) && filterer.IsObject()) {
			Scaleform::GFx::Value clamped;
			const std::array args{ Scaleform::GFx::Value(a_row) };
			if (filterer.Invoke("ClampIndex", &clamped, args)) {
				const auto row = AsInt(clamped);
				if (!row || *row < 0 || static_cast<std::uint32_t>(*row) >= DisplayRowCount(a_menu)) {
					return std::nullopt;  // nothing visible to select
				}
				a_row = *row;
			}
		}

		if (!index->owner.SetMember(index->member, Scaleform::GFx::Value(a_row))) {
			return std::nullopt;
		}
		return a_row;
	}

	std::optional<std::int32_t> ScrollPosition(RE::ExamineMenu* a_menu)
	{
		const auto list = ScrollList(a_menu);
		Scaleform::GFx::Value position;
		if (!list || !list->GetMember("scrollPosition", &position)) {
			return std::nullopt;
		}
		return AsInt(position);
	}

	void RestoreScrollPosition(RE::ExamineMenu* a_menu, std::int32_t a_position)
	{
		auto list = ScrollList(a_menu);
		Scaleform::GFx::Value max;
		if (!list || !list->GetMember("maxScrollPosition", &max)) {
			return;
		}
		// The setter ignores anything past the end, so clamp to the shorter list.
		const auto target = std::clamp(a_position, 0, AsInt(max).value_or(0));
		list->SetMember("scrollPosition", Scaleform::GFx::Value(static_cast<std::uint32_t>(target)));
	}

	std::optional<std::uint32_t> FindStackIndex(RE::TESBoundObject* a_object, std::uintptr_t a_identity)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->inventoryList) {
			return std::nullopt;
		}
		RE::BSAutoLock<RE::BSReadWriteLock, RE::BSAutoLockReadLockPolicy> lock{ player->inventoryList->rwLock };
		const auto* item = FindPlayerItem(a_object);
		if (!item) {
			return std::nullopt;
		}
		std::uint32_t index = 0;
		for (auto* stack = item->stackData.get(); stack; stack = stack->nextStack.get(), ++index) {
			if (reinterpret_cast<std::uintptr_t>(stack) == a_identity) {
				return index;
			}
		}
		return std::nullopt;
	}

	void RemoveFromStack(RE::TESBoundObject* a_object, std::uint32_t a_stackIndex, std::uint32_t a_count)
	{
		// Same call vanilla makes for a single scrap, with a larger count.
		RE::TESObjectREFR::RemoveItemData data{ a_object, static_cast<std::int32_t>(a_count) };
		data.stackData.push_back(a_stackIndex);
		// RemoveItem returns its ObjectRefHandle through a hidden pointer, which the
		// engine passes second: rcx = this, rdx = &handle, r8 = &data (see the call in
		// ScrapItemAccepted at B1925D). Typing it as a function returning the handle
		// would put that pointer first and shift every argument, so spell it out.
		RE::ObjectRefHandle dropped;
		CallVfunc(RE::PlayerCharacter::GetSingleton(), Slot::kRemoveItem, std::addressof(dropped), std::addressof(data));
	}
}
