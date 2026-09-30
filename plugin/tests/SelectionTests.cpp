#include <doctest/doctest.h>

#include "logic/Selection.h"

using ScrapStacks::RowAfterScrap;

TEST_CASE("the selection stays on the same row when the row still exists")
{
	CHECK(RowAfterScrap(5, 10) == 5);
	CHECK(RowAfterScrap(0, 3) == 0);
}

TEST_CASE("when the last row disappears, the selection moves up to the new last row")
{
	CHECK(RowAfterScrap(9, 9) == 8);
	CHECK(RowAfterScrap(12, 4) == 3);
}

TEST_CASE("an empty list has no selection")
{
	CHECK(RowAfterScrap(3, 0) == -1);
}

TEST_CASE("an unknown previous selection falls back to the top")
{
	CHECK(RowAfterScrap(-1, 5) == 0);
}
