#pragma once

#include "logic/PeImage.h"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ScrapStacks
{
	struct VTableInfo
	{
		std::uint32_t rva{ 0 };
		std::uint32_t subobjectOffset{ 0 };  // 0 = the class's primary vtable
	};

	// Finds a class's vtables from MSVC RTTI, with no Address Library involved:
	//   TypeDescriptor { void* vftable; void* spare; char name[] }       in .data
	//   CompleteObjectLocator { u32 signature = 1, u32 offset, u32 cdOffset,
	//     u32 typeDescriptor RVA, u32 classDescriptor RVA, u32 self RVA } in .rdata
	//   vtable[-1] = pointer to the CompleteObjectLocator                  in .rdata
	// a_namePrefix is the start of the mangled name, e.g. ".?AVExamineMenu@@".
	[[nodiscard]] inline std::vector<VTableInfo> FindVTables(const PeImage& a_image, std::string_view a_namePrefix)
	{
		std::vector<VTableInfo> found;
		const auto* data = a_image.FindSection(".data");
		const auto* rdata = a_image.FindSection(".rdata");
		if (!data || !rdata || a_namePrefix.empty()) {
			return found;
		}

		std::vector<std::uint32_t> typeDescriptors;
		const auto* first = data->data.data();
		const auto* last = first + data->data.size();
		for (auto* hit = std::search(first, last, a_namePrefix.begin(), a_namePrefix.end());
			hit != last;
			hit = std::search(hit + 1, last, a_namePrefix.begin(), a_namePrefix.end())) {
			const auto offset = static_cast<std::uint32_t>(hit - first);
			if (offset >= 0x10) {
				typeDescriptors.push_back(data->rva + offset - 0x10);
			}
		}
		if (typeDescriptors.empty()) {
			return found;
		}

		// Complete object locators pointing at those type descriptors, by their address.
		std::unordered_map<std::uint64_t, std::uint32_t> locators;  // absolute address -> subobject offset
		const auto& bytes = rdata->data;
		for (std::size_t pos = 0; pos + 24 <= bytes.size(); pos += 4) {
			std::uint32_t col[6];
			std::memcpy(col, bytes.data() + pos, sizeof(col));
			const auto rva = rdata->rva + static_cast<std::uint32_t>(pos);
			if (col[0] == 1 && col[5] == rva && std::ranges::find(typeDescriptors, col[3]) != typeDescriptors.end()) {
				locators.emplace(a_image.Base() + rva, col[1]);
			}
		}

		for (std::size_t pos = 0; pos + 8 <= bytes.size(); pos += 8) {
			std::uint64_t pointer;
			std::memcpy(&pointer, bytes.data() + pos, sizeof(pointer));
			if (const auto it = locators.find(pointer); it != locators.end()) {
				found.push_back({ rdata->rva + static_cast<std::uint32_t>(pos) + 8, it->second });
			}
		}
		return found;
	}
}
