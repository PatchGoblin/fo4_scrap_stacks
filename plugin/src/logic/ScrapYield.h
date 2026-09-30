#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace ScrapStacks
{
	template <class Key>
	struct YieldEntry
	{
		Key           item{};
		std::uint32_t count{ 0 };
	};

	// Adds one item's scrap yield into a running total, merging by component.
	//
	// A stack scrap runs the game's own per-item calculation once per copy and adds
	// the results together, rather than computing once and multiplying. Rounding in
	// that calculation happens per item, so this is what scrapping them one at a time
	// would have given.
	template <class Key>
	void AddYield(std::vector<YieldEntry<Key>>& a_total, std::span<const YieldEntry<Key>> a_one)
	{
		constexpr auto max = std::numeric_limits<std::uint32_t>::max();
		for (const auto& entry : a_one) {
			const auto it = std::ranges::find(a_total, entry.item, &YieldEntry<Key>::item);
			if (it == a_total.end()) {
				a_total.push_back(entry);
			} else {
				it->count = entry.count > max - it->count ? max : it->count + entry.count;
			}
		}
	}

	template <class Key>
	void AddYield(std::vector<YieldEntry<Key>>& a_total, const std::vector<YieldEntry<Key>>& a_one)
	{
		AddYield(a_total, std::span<const YieldEntry<Key>>{ a_one });
	}
}
