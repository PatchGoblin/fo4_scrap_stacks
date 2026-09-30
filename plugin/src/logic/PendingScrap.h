#pragma once

#include <cstdint>
#include <optional>

namespace ScrapStacks
{
	// Carries "this confirm is for a whole stack" from the Scrap press to the accept.
	//
	// Every Scrap press re-arms or disarms it, and accepting always consumes it, so a
	// cancelled stack scrap can never leak into a later single scrap. The row index is
	// checked on accept as a second guard.
	class PendingScrap
	{
	public:
		void Arm(std::uint32_t a_row, std::uint32_t a_copies)
		{
			_row = a_row;
			_copies = a_copies;
		}

		void Disarm() { _copies.reset(); }

		[[nodiscard]] std::optional<std::uint32_t> Take(std::uint32_t a_row)
		{
			const auto copies = _copies;
			_copies.reset();
			if (copies && _row == a_row) {
				return copies;
			}
			return std::nullopt;
		}

	private:
		std::uint32_t                _row{ 0 };
		std::optional<std::uint32_t> _copies;
	};
}
