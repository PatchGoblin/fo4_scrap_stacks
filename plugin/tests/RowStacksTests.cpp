#include <doctest/doctest.h>

#include "logic/RowStacks.h"

using ScrapStacks::PlanRemovals;
using ScrapStacks::RowStack;
using ScrapStacks::ScrappableCount;

TEST_CASE("equipped copies never count towards what a stack scrap takes")
{
	const std::vector<RowStack> row{
		{ .identity = 10, .index = 0, .count = 1, .equipped = true },
		{ .identity = 11, .index = 1, .count = 4, .equipped = false },
	};

	CHECK(ScrappableCount(row) == 4);
}

TEST_CASE("counts across several unequipped stacks in one row add up")
{
	const std::vector<RowStack> row{
		{ .identity = 10, .index = 0, .count = 2, .equipped = false },
		{ .identity = 11, .index = 3, .count = 3, .equipped = false },
	};

	CHECK(ScrappableCount(row) == 5);
}

TEST_CASE("removals take from the highest stack index first so lower indices stay valid")
{
	const std::vector<RowStack> row{
		{ .identity = 10, .index = 0, .count = 2, .equipped = false },
		{ .identity = 11, .index = 3, .count = 3, .equipped = false },
	};

	const auto plan = PlanRemovals(row, 4);

	REQUIRE(plan.size() == 2);
	CHECK(plan[0].identity == 11);
	CHECK(plan[0].count == 3);
	CHECK(plan[1].identity == 10);
	CHECK(plan[1].count == 1);
}

TEST_CASE("removals skip equipped stacks and never exceed what is there")
{
	const std::vector<RowStack> row{
		{ .identity = 10, .index = 0, .count = 1, .equipped = true },
		{ .identity = 11, .index = 1, .count = 2, .equipped = false },
	};

	const auto plan = PlanRemovals(row, 5);

	REQUIRE(plan.size() == 1);
	CHECK(plan[0].identity == 11);
	CHECK(plan[0].count == 2);
}

TEST_CASE("asking to remove nothing plans nothing")
{
	const std::vector<RowStack> row{ { .identity = 11, .index = 1, .count = 2, .equipped = false } };

	CHECK(PlanRemovals(row, 0).empty());
}

TEST_CASE("copies whose mods differ from the scrapped one are left in the row")
{
	const std::vector<RowStack> row{
		{ .identity = 10, .index = 0, .count = 3, .equipped = false, .sameModsAsScrapped = true },
		{ .identity = 11, .index = 1, .count = 5, .equipped = false, .sameModsAsScrapped = false },
		{ .identity = 12, .index = 2, .count = 2, .equipped = false, .sameModsAsScrapped = true },
	};

	CHECK(ScrappableCount(row) == 5);

	const auto plan = PlanRemovals(row, 4);
	REQUIRE(plan.size() == 2);
	CHECK(plan[0].identity == 12);
	CHECK(plan[0].count == 2);
	CHECK(plan[1].identity == 10);
	CHECK(plan[1].count == 2);
}

TEST_CASE("a stack scrap needs vanilla's own pick to match the scrapped copy")
{
	using ScrapStacks::VanillaPickMatches;

	// Vanilla removes its one copy from the first unequipped stack in the row.
	const std::vector<RowStack> ok{
		{ .identity = 10, .index = 0, .count = 1, .equipped = true, .sameModsAsScrapped = false },
		{ .identity = 11, .index = 1, .count = 4, .equipped = false, .sameModsAsScrapped = true },
	};
	const std::vector<RowStack> mismatch{
		{ .identity = 10, .index = 0, .count = 4, .equipped = false, .sameModsAsScrapped = false },
		{ .identity = 11, .index = 1, .count = 4, .equipped = false, .sameModsAsScrapped = true },
	};

	CHECK(VanillaPickMatches(ok));
	CHECK_FALSE(VanillaPickMatches(mismatch));
	CHECK_FALSE(VanillaPickMatches(std::vector<RowStack>{}));
}
