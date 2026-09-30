#pragma once

#include <algorithm>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace ScrapStacks
{
	// One attached mod as the game stores it (BGSMod::ObjectIndexData), copied field
	// by field. The game's struct has a padding byte after these, so comparing the
	// raw bytes compares leftover memory and can call identical copies different.
	struct ModEntry
	{
		std::uint32_t formID{ 0 };
		std::uint8_t  attachIndex{ 0 };
		std::uint8_t  rank{ 0 };
		std::uint8_t  disabled{ 0 };

		friend constexpr auto operator<=>(const ModEntry&, const ModEntry&) = default;
	};

	// True when both copies carry the same mods, in any order. Every copy scrapped
	// together must match, because the confirm dialog's yield comes from one of them.
	[[nodiscard]] inline bool SameMods(std::span<const ModEntry> a_lhs, std::span<const ModEntry> a_rhs)
	{
		if (a_lhs.size() != a_rhs.size()) {
			return false;
		}
		std::vector<ModEntry> lhs(a_lhs.begin(), a_lhs.end());
		std::vector<ModEntry> rhs(a_rhs.begin(), a_rhs.end());
		std::ranges::sort(lhs);
		std::ranges::sort(rhs);
		return lhs == rhs;
	}

	inline bool SameMods(const std::vector<ModEntry>& a_lhs, const std::vector<ModEntry>& a_rhs)
	{
		return SameMods(std::span<const ModEntry>{ a_lhs }, std::span<const ModEntry>{ a_rhs });
	}

	[[nodiscard]] inline std::string Describe(std::span<const ModEntry> a_mods)
	{
		std::string text = "[";
		for (const auto& mod : a_mods) {
			text += std::format("{}{:08X}@{} r{}{}", text.size() > 1 ? ", " : "", mod.formID, mod.attachIndex, mod.rank, mod.disabled ? " off" : "");
		}
		return text + "]";
	}

	inline std::string Describe(const std::vector<ModEntry>& a_mods)
	{
		return Describe(std::span<const ModEntry>{ a_mods });
	}
}
