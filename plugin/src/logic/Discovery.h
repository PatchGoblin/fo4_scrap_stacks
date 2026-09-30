#pragma once

#include "logic/AddressLibrary.h"
#include "logic/Pattern.h"
#include "logic/PeImage.h"
#include "logic/Rtti.h"

#include <cstdint>
#include <charconv>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ScrapStacks
{
	// Finds everything the scrap hooks need in a Fallout4.exe image, on any build,
	// and checks every assumption the hooks make. Nothing is patched unless all of it
	// holds; on failure the plugin logs the reasons and stays inert.
	//
	// Anchors, all verified against every build in docs/versions.md (tests/ExeDiscoveryTests):
	//  - the ExamineMenu and scrap-callback vtables, by RTTI class name
	//  - the three call sites, by byte patterns that must match exactly once
	//  - call targets and struct offsets, cross-checked against Address Library

	// Address Library IDs as {old-gen, next-gen and AE}. The plugin resolves these
	// through CommonLib; the offline tests read them from the database files.
	struct AddressId
	{
		std::uint64_t oldGen;
		std::uint64_t nextGen;
	};
	inline constexpr AddressId kBuildYieldId{ 646841, 2223077 };         // ExamineMenu::BuildWeaponScrappingArray
	inline constexpr AddressId kShowConfirmId{ 443081, 2223081 };        // ExamineMenu::ShowConfirmMenu
	inline constexpr AddressId kUpdateAutoBuildId{ 769581, 2224955 };    // WorkbenchMenuBase::UpdateOptimizedAutoBuildInv
	inline constexpr AddressId kGetSelectedIndexId{ 776503, 2223022 };  // ExamineMenu::GetSelectedIndex

	// RVAs of the functions Address Library names, for cross-checking.
	struct AddressHints
	{
		std::optional<std::uint32_t> buildYield;
		std::optional<std::uint32_t> showConfirm;
		std::optional<std::uint32_t> updateAutoBuild;
		std::optional<std::uint32_t> getSelectedIndex;
	};

	struct HookSites
	{
		std::uint32_t examineMenuVtbl{ 0 };
		std::uint32_t scrapCallbackVtbl{ 0 };
		std::uint32_t yieldCall{ 0 };    // call BuildWeaponScrappingArray, in ExamineMenu::Call's ScrapItem case
		std::uint32_t labelCall{ 0 };    // call InitDataScrap ctor, same case
		std::uint32_t removalCall{ 0 };  // call UpdateOptimizedAutoBuildInv, in ScrapItemAccepted after RemoveItem
	};

	struct DiscoveryResult
	{
		std::optional<HookSites> sites;
		std::vector<std::string> failures;
	};

	// "1.10.163" and older are old-gen; 1.10.980 and later use the next-gen IDs.
	[[nodiscard]] inline bool IsOldGen(std::string_view a_version)
	{
		unsigned parts[3]{};
		const char* at = a_version.data();
		const char* end = a_version.data() + a_version.size();
		for (std::size_t i = 0; i < 3; ++i) {
			const auto [next, error] = std::from_chars(at, end, parts[i]);
			if (error != std::errc{} || (i < 2 && (next == end || *next != '.'))) {
				return false;
			}
			at = next + (i < 2 ? 1 : 0);
		}
		return parts[0] == 1 && (parts[1] < 10 || (parts[1] == 10 && parts[2] < 980));
	}

	[[nodiscard]] inline AddressHints HintsFromLibrary(const AddressLibrary& a_library, bool a_oldGen)
	{
		const auto pick = [&](const AddressId& a_id) { return a_library.Offset(a_oldGen ? a_id.oldGen : a_id.nextGen); };
		return { pick(kBuildYieldId), pick(kShowConfirmId), pick(kUpdateAutoBuildId), pick(kGetSelectedIndexId) };
	}

	namespace Detail
	{
		// Each pattern must match exactly once in its window.
		inline constexpr std::string_view kYieldPattern = "49 8B CE E8 ?? ?? ?? ?? B9 18 00 00 00 E8 ?? ?? ?? ?? 33 FF 48 8B F0 48 85 C0";
		inline constexpr std::size_t      kYieldCallOffset = 3;
		inline constexpr std::string_view kLabelPattern = "E8 ?? ?? ?? ?? 48 8B F8 4C 8B C6 48 8B D7 49 8B CE E8";
		inline constexpr std::size_t      kLabelShowConfirmOffset = 17;
		inline constexpr std::string_view kRemovalPattern = "FF 90 68 03 00 00 48 8B ?? E8 ?? ?? ?? ??";  // call [rax+0x368] = RemoveItem
		inline constexpr std::size_t      kRemovalCallOffset = 9;
		inline constexpr std::string_view kAcceptThunk = "8B 51 10 48 8B 49 08 E9";  // mov edx,[rcx+10h]; mov rcx,[rcx+8]; jmp

		inline constexpr std::size_t kCallWindow = 0x1000;
		inline constexpr std::size_t kAcceptedWindow = 0x300;
		inline constexpr std::size_t kBuildYieldWindow = 0x400;
		inline constexpr std::size_t kGetSelectedIndexWindow = 0x40;

		// Struct offsets and vtable slots the plugin relies on, each confirmed by an
		// instruction that uses it.
		struct LayoutCheck
		{
			std::string_view what;
			std::string_view pattern;
		};
		inline constexpr LayoutCheck kAcceptedLayout[]{
			{ "ExamineMenu::scrappingArray size at +0x438", "8B 81 38 04 00 00" },
			{ "ExamineMenu::scrappingArray data at +0x428", "48 8B 99 28 04 00 00" },
			{ "stackedEntries data at +0x3D0", "48 03 ?? D0 03 00 00" },
			{ "stackedEntries size at +0x3E0", "44 3B ?? E0 03 00 00" },
			{ "entriesInvalid at +0x3E8", "44 38 ?? E8 03 00 00" },
			{ "BGSInventoryItem::Stack flags at +0x24", "41 F6 ?? 24 07" },
		};
		inline constexpr LayoutCheck kBuildYieldLayout{ "GetObjectInstanceExtra at vtable slot 0x26", "FF 90 30 01 00 00" };
		inline constexpr LayoutCheck kGetSelectedIndexLayout{ "ExamineMenu::itemList at +0x488", "48 8B 89 88 04 00 00" };

		inline std::vector<std::size_t> Find(const PeImage& a_image, std::uint32_t a_rva, std::size_t a_window, std::string_view a_pattern)
		{
			return Pattern::Parse(a_pattern)->FindAll(a_image.Bytes(a_rva, a_window));
		}

		inline std::optional<std::uint32_t> UniqueVTable(const PeImage& a_image, std::string_view a_name, std::vector<std::string>& a_failures)
		{
			std::vector<VTableInfo> primary;
			for (const auto& vtable : FindVTables(a_image, a_name)) {
				if (vtable.subobjectOffset == 0) {
					primary.push_back(vtable);
				}
			}
			if (primary.size() != 1) {
				a_failures.push_back(std::format("RTTI: expected one primary vtable for {}, found {}", a_name, primary.size()));
				return std::nullopt;
			}
			return primary.front().rva;
		}

		inline std::optional<std::size_t> UniqueHit(const PeImage& a_image, std::string_view a_what, std::uint32_t a_rva, std::size_t a_window,
			std::string_view a_pattern, std::vector<std::string>& a_failures)
		{
			const auto hits = Find(a_image, a_rva, a_window, a_pattern);
			if (hits.size() != 1) {
				a_failures.push_back(std::format("{}: pattern matched {} times near {:#x}, expected once", a_what, hits.size(), a_rva));
				return std::nullopt;
			}
			return hits.front();
		}

		inline void CrossCheck(std::string_view a_what, std::optional<std::uint32_t> a_found, std::optional<std::uint32_t> a_expected,
			std::vector<std::string>& a_failures)
		{
			if (!a_expected) {
				a_failures.push_back(std::format("{}: Address Library has no entry to check against", a_what));
			} else if (a_found != a_expected) {
				a_failures.push_back(std::format("{}: found {:#x}, Address Library says {:#x}", a_what, a_found.value_or(0), *a_expected));
			}
		}
	}

	[[nodiscard]] inline DiscoveryResult Discover(const PeImage& a_image, const AddressHints& a_hints)
	{
		using namespace Detail;
		DiscoveryResult result;
		auto& failures = result.failures;

		const auto examineMenu = UniqueVTable(a_image, ".?AVExamineMenu@@", failures);
		const auto scrapCallback = UniqueVTable(a_image, ".?AVScrapItemCallback@?A0x", failures);
		if (!examineMenu || !scrapCallback) {
			return result;
		}

		// ExamineMenu::Call (vtable slot 1) holds the ScrapItem case.
		const auto call = a_image.PointerAt(*examineMenu + 8);
		// The callback's OnAccept (slot 1) is a thunk that jumps to ScrapItemAccepted.
		const auto onAccept = a_image.PointerAt(*scrapCallback + 8);
		if (!call || !onAccept) {
			failures.push_back("vtable slot 1 of ExamineMenu or the scrap callback is not a pointer into the image");
			return result;
		}
		if (Find(a_image, *onAccept, 8, kAcceptThunk) != std::vector<std::size_t>{ 0 }) {
			failures.push_back(std::format("scrap callback OnAccept at {:#x} is not the expected thunk", *onAccept));
			return result;
		}
		const auto accepted = a_image.Rel32Target(*onAccept + 7, 0xE9);

		HookSites sites{ .examineMenuVtbl = *examineMenu, .scrapCallbackVtbl = *scrapCallback };
		std::optional<std::uint32_t> buildYield;
		if (const auto hit = UniqueHit(a_image, "yield call site", *call, kCallWindow, kYieldPattern, failures)) {
			sites.yieldCall = *call + static_cast<std::uint32_t>(*hit + kYieldCallOffset);
			buildYield = a_image.Rel32Target(sites.yieldCall, 0xE8);
			CrossCheck("BuildWeaponScrappingArray", buildYield, a_hints.buildYield, failures);
		}
		if (const auto hit = UniqueHit(a_image, "dialog label call site", *call, kCallWindow, kLabelPattern, failures)) {
			sites.labelCall = *call + static_cast<std::uint32_t>(*hit);
			CrossCheck("ShowConfirmMenu after the label call", a_image.Rel32Target(sites.labelCall + kLabelShowConfirmOffset, 0xE8), a_hints.showConfirm, failures);
			if (sites.yieldCall && (sites.labelCall < sites.yieldCall || sites.labelCall - sites.yieldCall > 0x100)) {
				failures.push_back("dialog label call site is not in the same ScrapItem case as the yield call");
			}
		}
		if (!accepted) {
			failures.push_back("scrap callback thunk has no jump target");
		} else if (const auto hit = UniqueHit(a_image, "copy removal call site", *accepted, kAcceptedWindow, kRemovalPattern, failures)) {
			sites.removalCall = *accepted + static_cast<std::uint32_t>(*hit + kRemovalCallOffset);
			CrossCheck("UpdateOptimizedAutoBuildInv", a_image.Rel32Target(sites.removalCall, 0xE8), a_hints.updateAutoBuild, failures);
		}

		if (accepted) {
			for (const auto& check : kAcceptedLayout) {
				if (Find(a_image, *accepted, kAcceptedWindow, check.pattern).empty()) {
					failures.push_back(std::format("layout: no instruction confirms {}", check.what));
				}
			}
		}
		if (buildYield && Find(a_image, *buildYield, kBuildYieldWindow, kBuildYieldLayout.pattern).empty()) {
			failures.push_back(std::format("layout: no instruction confirms {}", kBuildYieldLayout.what));
		}
		if (!a_hints.getSelectedIndex) {
			failures.push_back("GetSelectedIndex: Address Library has no entry");
		} else if (Find(a_image, *a_hints.getSelectedIndex, kGetSelectedIndexWindow, kGetSelectedIndexLayout.pattern).empty()) {
			failures.push_back(std::format("layout: no instruction confirms {}", kGetSelectedIndexLayout.what));
		}

		if (failures.empty()) {
			result.sites = sites;
		}
		return result;
	}
}
