#include <doctest/doctest.h>

#include "logic/DialogLabel.h"

using ScrapStacks::LabelSuffix;
using ScrapStacks::StackDecision;
using Kind = StackDecision::Kind;

TEST_CASE("a plain scrap leaves the item name alone")
{
	CHECK(LabelSuffix({ Kind::kNone }).empty());
}

TEST_CASE("a whole-row stack scrap shows the copy count")
{
	CHECK(LabelSuffix({ Kind::kStack, 12, 12 }) == " (x12)");
}

TEST_CASE("a partial stack scrap says the rest have other mods")
{
	CHECK(LabelSuffix({ Kind::kStack, 12, 17 }) == " (x12 of 17, rest have other mods)");
}

TEST_CASE("a refused stack scrap says why it took one copy")
{
	CHECK(LabelSuffix({ Kind::kFavorite }) == " (x1, stack has a favorite)");
	CHECK(LabelSuffix({ Kind::kMixedMods }) == " (x1, copies have different mods)");
}
