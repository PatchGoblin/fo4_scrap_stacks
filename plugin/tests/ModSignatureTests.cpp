#include <doctest/doctest.h>

#include "logic/ModSignature.h"

using ScrapStacks::ModEntry;
using ScrapStacks::SameMods;

TEST_CASE("copies with the same mods match")
{
	const std::vector<ModEntry> a{ { 0x100, 0, 1, 0 }, { 0x200, 1, 1, 0 } };
	const std::vector<ModEntry> b{ { 0x100, 0, 1, 0 }, { 0x200, 1, 1, 0 } };

	CHECK(SameMods(a, b));
}

TEST_CASE("the same mods stored in a different order still match")
{
	const std::vector<ModEntry> a{ { 0x100, 0, 1, 0 }, { 0x200, 1, 1, 0 } };
	const std::vector<ModEntry> b{ { 0x200, 1, 1, 0 }, { 0x100, 0, 1, 0 } };

	CHECK(SameMods(a, b));
}

TEST_CASE("a different mod, rank or disabled flag is a real difference")
{
	const std::vector<ModEntry> base{ { 0x100, 0, 1, 0 } };

	CHECK_FALSE(SameMods(base, std::vector<ModEntry>{ { 0x101, 0, 1, 0 } }));
	CHECK_FALSE(SameMods(base, std::vector<ModEntry>{ { 0x100, 0, 2, 0 } }));
	CHECK_FALSE(SameMods(base, std::vector<ModEntry>{ { 0x100, 0, 1, 1 } }));
	CHECK_FALSE(SameMods(base, std::vector<ModEntry>{}));
}

TEST_CASE("a mod listed twice is not the same as listed once")
{
	const std::vector<ModEntry> once{ { 0x100, 0, 1, 0 } };
	const std::vector<ModEntry> twice{ { 0x100, 0, 1, 0 }, { 0x100, 0, 1, 0 } };

	CHECK_FALSE(SameMods(once, twice));
}

TEST_CASE("mod lists describe themselves for the log")
{
	const std::vector<ModEntry> mods{ { 0x1A2B, 3, 1, 0 } };

	CHECK(ScrapStacks::Describe(mods) == "[00001A2B@3 r1]");
	CHECK(ScrapStacks::Describe(std::vector<ModEntry>{}) == "[]");
}
