#include <doctest/doctest.h>

#include "logic/AddressLibrary.h"

#include <cstring>
#include <utility>
#include <vector>

using ScrapStacks::AddressLibrary;

namespace
{
	std::vector<std::uint8_t> Database(const std::vector<std::pair<std::uint64_t, std::uint64_t>>& a_pairs)
	{
		std::vector<std::uint8_t> bytes(8 + a_pairs.size() * 16);
		const std::uint64_t count = a_pairs.size();
		std::memcpy(bytes.data(), &count, 8);
		for (std::size_t i = 0; i < a_pairs.size(); ++i) {
			std::memcpy(bytes.data() + 8 + i * 16, &a_pairs[i].first, 8);
			std::memcpy(bytes.data() + 16 + i * 16, &a_pairs[i].second, 8);
		}
		return bytes;
	}
}

TEST_CASE("IDs resolve to their offsets")
{
	const auto library = AddressLibrary::Parse(Database({ { 5, 0x1000 }, { 9, 0x20 }, { 646841, 0xB20C30 } }));
	REQUIRE(library.has_value());

	CHECK(library->Offset(5) == 0x1000u);
	CHECK(library->Offset(646841) == 0xB20C30u);
}

TEST_CASE("a missing ID is reported as missing, never as a neighbour's offset")
{
	const auto library = AddressLibrary::Parse(Database({ { 5, 0x1000 }, { 9, 0x20 } }));
	REQUIRE(library.has_value());

	CHECK_FALSE(library->Offset(7).has_value());
	CHECK_FALSE(library->Offset(1).has_value());
	CHECK_FALSE(library->Offset(10).has_value());
}

TEST_CASE("truncated databases are rejected")
{
	auto bytes = Database({ { 5, 0x1000 }, { 9, 0x20 } });
	bytes.resize(bytes.size() - 1);

	CHECK_FALSE(AddressLibrary::Parse(bytes).has_value());
	CHECK_FALSE(AddressLibrary::Parse(std::vector<std::uint8_t>{ 1, 2, 3 }).has_value());
}
