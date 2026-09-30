#include <doctest/doctest.h>

#include "logic/PendingScrap.h"

using ScrapStacks::PendingScrap;

TEST_CASE("nothing is pending until a stack scrap is armed")
{
	PendingScrap pending;

	CHECK_FALSE(pending.Take(3).has_value());
}

TEST_CASE("an armed stack scrap is taken once, for the row it was armed on")
{
	PendingScrap pending;
	pending.Arm(3, 5);

	const auto copies = pending.Take(3);

	REQUIRE(copies.has_value());
	CHECK(*copies == 5);
	CHECK_FALSE(pending.Take(3).has_value());
}

TEST_CASE("accepting a scrap on a different row does not inherit a stale stack scrap")
{
	PendingScrap pending;
	pending.Arm(3, 5);

	CHECK_FALSE(pending.Take(4).has_value());
	CHECK_FALSE(pending.Take(3).has_value());
}

TEST_CASE("a cancelled confirm is replaced by the next scrap press")
{
	PendingScrap pending;
	pending.Arm(3, 5);
	pending.Disarm();  // next plain Scrap press

	CHECK_FALSE(pending.Take(3).has_value());
}
