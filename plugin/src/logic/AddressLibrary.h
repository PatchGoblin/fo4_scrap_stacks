#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace ScrapStacks
{
	// Reads an F4SE Address Library database (version-*.bin, the "V0" format every
	// F4SE runtime uses): a u64 count, then (u64 id, u64 offset) pairs sorted by id.
	//
	// The plugin reads the file itself rather than asking CommonLib, whose lookup
	// shows a fatal error box on some failures and, for an ID that isn't there,
	// returns the next ID's offset. Here a missing ID is simply missing.
	class AddressLibrary
	{
	public:
		[[nodiscard]] static std::optional<AddressLibrary> Parse(std::vector<std::uint8_t> a_bytes)
		{
			std::uint64_t count = 0;
			if (a_bytes.size() < sizeof(count)) {
				return std::nullopt;
			}
			std::memcpy(&count, a_bytes.data(), sizeof(count));
			if ((a_bytes.size() - sizeof(count)) / kPairSize < count) {
				return std::nullopt;
			}
			AddressLibrary library;
			library._count = static_cast<std::size_t>(count);
			library._bytes = std::move(a_bytes);
			return library;
		}

		[[nodiscard]] std::optional<std::uint32_t> Offset(std::uint64_t a_id) const
		{
			std::size_t low = 0;
			std::size_t high = _count;
			while (low < high) {
				const auto mid = low + (high - low) / 2;
				const auto id = Field(mid, 0);
				if (id == a_id) {
					return static_cast<std::uint32_t>(Field(mid, 1));
				}
				if (id < a_id) {
					low = mid + 1;
				} else {
					high = mid;
				}
			}
			return std::nullopt;
		}

	private:
		static constexpr std::size_t kPairSize = 16;

		std::uint64_t Field(std::size_t a_pair, std::size_t a_field) const
		{
			std::uint64_t value;
			std::memcpy(&value, _bytes.data() + sizeof(std::uint64_t) + a_pair * kPairSize + a_field * sizeof(std::uint64_t), sizeof(value));
			return value;
		}

		std::vector<std::uint8_t> _bytes;
		std::size_t               _count{ 0 };
	};
}
