#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ScrapStacks
{
	// Read-only view of a PE image, addressed by RVA. The same code reads the game
	// running in memory (the plugin) or an exe file on disk (the offline tests), so
	// discovery is exercised identically in both.
	class PeImage
	{
	public:
		enum class Layout
		{
			kFile,    // bytes are the file: sections at PointerToRawData
			kMapped,  // bytes are the loaded image: sections at their RVA
		};

		struct Section
		{
			std::string                    name;
			std::uint32_t                  rva{ 0 };
			std::span<const std::uint8_t>  data;
		};

		// a_loadedBase is the address the image is loaded at (kMapped); absolute
		// pointers inside a loaded image are relocated to it. For a file the preferred
		// ImageBase from the header is used.
		[[nodiscard]] static std::optional<PeImage> Parse(std::span<const std::uint8_t> a_bytes, Layout a_layout, std::uint64_t a_loadedBase = 0)
		{
			const auto u16 = [&](std::size_t a_at) -> std::optional<std::uint16_t> { return Read<std::uint16_t>(a_bytes, a_at); };
			const auto u32 = [&](std::size_t a_at) -> std::optional<std::uint32_t> { return Read<std::uint32_t>(a_bytes, a_at); };

			if (u16(0) != 0x5A4D) {  // "MZ"
				return std::nullopt;
			}
			const auto nt = u32(0x3C);
			if (!nt || u32(*nt) != 0x00004550) {  // "PE\0\0"
				return std::nullopt;
			}
			const auto sectionCount = u16(*nt + 6);
			const auto optionalSize = u16(*nt + 20);
			const auto magic = u16(*nt + 24);
			if (!sectionCount || !optionalSize || magic != 0x20B) {  // PE32+ only
				return std::nullopt;
			}
			const auto imageBase = Read<std::uint64_t>(a_bytes, *nt + 24 + 24);
			if (!imageBase) {
				return std::nullopt;
			}

			PeImage image;
			image._base = a_layout == Layout::kMapped ? a_loadedBase : *imageBase;
			const std::size_t table = *nt + 24 + *optionalSize;
			for (std::uint16_t i = 0; i < *sectionCount; ++i) {
				const std::size_t header = table + i * 40u;
				if (header + 40 > a_bytes.size()) {
					return std::nullopt;
				}
				char rawName[9]{};
				std::memcpy(rawName, a_bytes.data() + header, 8);
				const auto virtualSize = *u32(header + 8);
				const auto rva = *u32(header + 12);
				const auto rawSize = *u32(header + 16);
				const auto rawOffset = *u32(header + 20);

				const std::size_t start = a_layout == Layout::kMapped ? rva : rawOffset;
				const std::size_t size = a_layout == Layout::kMapped ? virtualSize : std::min(rawSize, virtualSize ? virtualSize : rawSize);
				if (start > a_bytes.size()) {
					return std::nullopt;
				}
				image._sections.push_back({ rawName, rva, a_bytes.subspan(start, std::min(size, a_bytes.size() - start)) });
			}
			return image;
		}

		[[nodiscard]] std::uint64_t Base() const { return _base; }

		[[nodiscard]] const Section* FindSection(std::string_view a_name) const
		{
			for (const auto& section : _sections) {
				if (section.name == a_name) {
					return &section;
				}
			}
			return nullptr;
		}

		// Bytes at [a_rva, a_rva + a_size), clipped to the containing section. Empty if
		// a_rva isn't inside any section.
		[[nodiscard]] std::span<const std::uint8_t> Bytes(std::uint32_t a_rva, std::size_t a_size) const
		{
			for (const auto& section : _sections) {
				if (a_rva >= section.rva && a_rva - section.rva < section.data.size()) {
					const auto offset = a_rva - section.rva;
					return section.data.subspan(offset, std::min(a_size, section.data.size() - offset));
				}
			}
			return {};
		}

		template <class T>
		[[nodiscard]] std::optional<T> Value(std::uint32_t a_rva) const
		{
			return Read<T>(Bytes(a_rva, sizeof(T)), 0);
		}

		// An absolute pointer stored at a_rva, converted to an RVA.
		[[nodiscard]] std::optional<std::uint32_t> PointerAt(std::uint32_t a_rva) const
		{
			const auto pointer = Value<std::uint64_t>(a_rva);
			if (!pointer || *pointer < _base || *pointer - _base > 0xFFFFFFFFull) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(*pointer - _base);
		}

		// Target of a 5-byte rel32 call/jmp whose opcode is at a_rva.
		[[nodiscard]] std::optional<std::uint32_t> Rel32Target(std::uint32_t a_rva, std::uint8_t a_opcode) const
		{
			if (Value<std::uint8_t>(a_rva) != a_opcode) {
				return std::nullopt;
			}
			const auto rel = Value<std::int32_t>(a_rva + 1);
			if (!rel) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(static_cast<std::int64_t>(a_rva) + 5 + *rel);
		}

	private:
		template <class T>
		static std::optional<T> Read(std::span<const std::uint8_t> a_bytes, std::size_t a_at)
		{
			if (a_at > a_bytes.size() || a_bytes.size() - a_at < sizeof(T)) {
				return std::nullopt;
			}
			T value;
			std::memcpy(&value, a_bytes.data() + a_at, sizeof(T));
			return value;
		}

		std::uint64_t        _base{ 0 };
		std::vector<Section> _sections;
	};
}
