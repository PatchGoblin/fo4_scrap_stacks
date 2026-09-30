#include <doctest/doctest.h>

#include "logic/ScrapYield.h"

#include <limits>

using Entry = ScrapStacks::YieldEntry<int>;
using ScrapStacks::AddYield;

TEST_CASE("adding one item's yield to an empty total copies it")
{
	std::vector<Entry> total;
	const std::vector<Entry> one{ { 1, 2 }, { 7, 1 } };

	AddYield(total, one);

	REQUIRE(total.size() == 2);
	CHECK(total[0].item == 1);
	CHECK(total[0].count == 2);
	CHECK(total[1].item == 7);
	CHECK(total[1].count == 1);
}

TEST_CASE("per-item yields are summed by component, keeping first-seen order")
{
	std::vector<Entry> total;
	AddYield(total, std::vector<Entry>{ { 1, 2 }, { 7, 1 } });
	AddYield(total, std::vector<Entry>{ { 7, 3 }, { 9, 4 }, { 1, 2 } });

	REQUIRE(total.size() == 3);
	CHECK(total[0].item == 1);
	CHECK(total[0].count == 4);
	CHECK(total[1].item == 7);
	CHECK(total[1].count == 4);
	CHECK(total[2].item == 9);
	CHECK(total[2].count == 4);
}

TEST_CASE("a component that yields nothing for one item does not disturb the others")
{
	std::vector<Entry> total;
	AddYield(total, std::vector<Entry>{ { 1, 0 }, { 2, 5 } });
	AddYield(total, std::vector<Entry>{ { 2, 5 } });

	REQUIRE(total.size() == 2);
	CHECK(total[0].count == 0);
	CHECK(total[1].count == 10);
}

TEST_CASE("sums saturate instead of wrapping around")
{
	constexpr auto max = std::numeric_limits<std::uint32_t>::max();
	std::vector<Entry> total{ { 1, max - 1 } };

	AddYield(total, std::vector<Entry>{ { 1, 5 } });

	CHECK(total[0].count == max);
}
