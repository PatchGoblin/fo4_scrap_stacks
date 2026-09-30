#pragma once

#include <cstddef>
#include <cstdint>

namespace ScrapStacks
{
	// Which list row to select after a scrap. Vanilla always selects row 0; this keeps
	// the cursor where it was, moving up only when the list got shorter than that.
	[[nodiscard]] constexpr std::int32_t RowAfterScrap(std::int32_t a_previous, std::size_t a_newCount)
	{
		if (a_newCount == 0) {
			return -1;
		}
		if (a_previous < 0) {
			return 0;
		}
		const auto last = static_cast<std::int32_t>(a_newCount - 1);
		return a_previous > last ? last : a_previous;
	}
}
