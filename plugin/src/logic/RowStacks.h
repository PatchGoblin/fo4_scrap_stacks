#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace ScrapStacks
{
	// One inventory stack behind a workbench list row. A row can stand for several
	// engine stacks of the same item.
	struct RowStack
	{
		std::uintptr_t identity{ 0 };  // the stack object's address: stable while it exists
		std::uint32_t  index{ 0 };     // position in the item's stack list, used for removal
		std::uint32_t  count{ 0 };
		bool           equipped{ false };
		// Carries the same mods as the copy the yield is computed from. The game can
		// show copies with different mods in one list row; only matching ones scrap
		// together, so every scrapped copy gets exactly the yield it would alone.
		bool sameModsAsScrapped{ true };
	};

	[[nodiscard]] constexpr bool Scrappable(const RowStack& a_stack)
	{
		return !a_stack.equipped && a_stack.sameModsAsScrapped;
	}

	struct Removal
	{
		std::uintptr_t identity{ 0 };
		std::uint32_t  count{ 0 };
	};

	// Copies a stack scrap may take from a row: unequipped (vanilla never scraps an
	// equipped copy either) and carrying the same mods as the scrapped copy.
	[[nodiscard]] inline std::uint32_t ScrappableCount(std::span<const RowStack> a_row)
	{
		std::uint32_t total = 0;
		for (const auto& stack : a_row) {
			if (Scrappable(stack)) {
				total += stack.count;
			}
		}
		return total;
	}

	// Which stacks to take a_count copies from. Highest stack index first: removing
	// a stack entirely shifts only the indices after it, so the ones still to be
	// removed keep their positions.
	[[nodiscard]] inline std::vector<Removal> PlanRemovals(std::span<const RowStack> a_row, std::uint32_t a_count)
	{
		std::vector<RowStack> candidates;
		for (const auto& stack : a_row) {
			if (Scrappable(stack) && stack.count > 0) {
				candidates.push_back(stack);
			}
		}
		std::ranges::sort(candidates, std::ranges::greater{}, &RowStack::index);

		std::vector<Removal> plan;
		for (const auto& stack : candidates) {
			if (a_count == 0) {
				break;
			}
			const auto take = std::min(a_count, stack.count);
			plan.push_back({ stack.identity, take });
			a_count -= take;
		}
		return plan;
	}

	// Vanilla removes its one copy from the first unequipped stack in the row. A stack
	// scrap is only safe when that copy is one of the matching ones, or the summed
	// yield would include a copy that was never scrapped.
	[[nodiscard]] inline bool VanillaPickMatches(std::span<const RowStack> a_row)
	{
		const auto first = std::ranges::find_if(a_row, [](const RowStack& a_stack) { return !a_stack.equipped; });
		return first != a_row.end() && first->sameModsAsScrapped;
	}

	inline bool VanillaPickMatches(const std::vector<RowStack>& a_row)
	{
		return VanillaPickMatches(std::span<const RowStack>{ a_row });
	}

	inline std::vector<Removal> PlanRemovals(const std::vector<RowStack>& a_row, std::uint32_t a_count)
	{
		return PlanRemovals(std::span<const RowStack>{ a_row }, a_count);
	}

	inline std::uint32_t ScrappableCount(const std::vector<RowStack>& a_row)
	{
		return ScrappableCount(std::span<const RowStack>{ a_row });
	}
}
