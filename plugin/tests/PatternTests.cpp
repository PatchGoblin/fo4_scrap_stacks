#include <doctest/doctest.h>

#include "logic/Pattern.h"

#include <array>

using ScrapStacks::Pattern;

TEST_CASE("patterns parse hex bytes and ?? wildcards")
{
	const auto pattern = Pattern::Parse("E8 ?? ?? ?? ?? B9 18");
	REQUIRE(pattern.has_value());
	CHECK(pattern->Size() == 7);
}

TEST_CASE("malformed patterns are rejected")
{
	CHECK_FALSE(Pattern::Parse("").has_value());
	CHECK_FALSE(Pattern::Parse("E8 G1").has_value());
	CHECK_FALSE(Pattern::Parse("E8 123").has_value());
}

TEST_CASE("wildcards match any byte and fixed bytes must match exactly")
{
	constexpr std::array<std::uint8_t, 10> code{ 0x90, 0xE8, 0x01, 0x02, 0x03, 0x04, 0xB9, 0x18, 0xE8, 0x00 };
	const auto pattern = *Pattern::Parse("E8 ?? ?? ?? ?? B9 18");

	const auto hits = pattern.FindAll(code);

	REQUIRE(hits.size() == 1);
	CHECK(hits[0] == 1);
}

TEST_CASE("every occurrence is reported so callers can insist on exactly one")
{
	constexpr std::array<std::uint8_t, 6> code{ 0xAA, 0xBB, 0x00, 0xAA, 0xBB, 0xAA };
	const auto pattern = *Pattern::Parse("AA BB");

	const auto hits = pattern.FindAll(code);

	REQUIRE(hits.size() == 2);
	CHECK(hits[0] == 0);
	CHECK(hits[1] == 3);
}

TEST_CASE("a pattern longer than the data finds nothing")
{
	constexpr std::array<std::uint8_t, 2> code{ 0xAA, 0xBB };

	CHECK(Pattern::Parse("AA BB CC")->FindAll(code).empty());
}
