#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ScrapStacks
{
	// A byte pattern such as "E8 ?? ?? ?? ?? B9 18", where ?? matches any byte.
	class Pattern
	{
	public:
		[[nodiscard]] static std::optional<Pattern> Parse(std::string_view a_text)
		{
			Pattern pattern;
			std::size_t pos = 0;
			while (pos < a_text.size()) {
				if (a_text[pos] == ' ') {
					++pos;
					continue;
				}
				const auto token = a_text.substr(pos, a_text.find(' ', pos) - pos);
				pos += token.size();
				if (token == "??") {
					pattern._bytes.push_back(-1);
					continue;
				}
				if (token.size() != 2) {
					return std::nullopt;
				}
				const auto hi = HexDigit(token[0]);
				const auto lo = HexDigit(token[1]);
				if (hi < 0 || lo < 0) {
					return std::nullopt;
				}
				pattern._bytes.push_back(static_cast<std::int16_t>(hi * 16 + lo));
			}
			if (pattern._bytes.empty()) {
				return std::nullopt;
			}
			return pattern;
		}

		[[nodiscard]] std::size_t Size() const { return _bytes.size(); }

		// Offsets of every match in a_data, in order.
		[[nodiscard]] std::vector<std::size_t> FindAll(std::span<const std::uint8_t> a_data) const
		{
			std::vector<std::size_t> hits;
			if (a_data.size() < _bytes.size()) {
				return hits;
			}
			for (std::size_t start = 0; start + _bytes.size() <= a_data.size(); ++start) {
				bool match = true;
				for (std::size_t i = 0; i < _bytes.size(); ++i) {
					if (_bytes[i] >= 0 && a_data[start + i] != static_cast<std::uint8_t>(_bytes[i])) {
						match = false;
						break;
					}
				}
				if (match) {
					hits.push_back(start);
				}
			}
			return hits;
		}

	private:
		static int HexDigit(char a_char)
		{
			if (a_char >= '0' && a_char <= '9') {
				return a_char - '0';
			}
			if (a_char >= 'A' && a_char <= 'F') {
				return a_char - 'A' + 10;
			}
			if (a_char >= 'a' && a_char <= 'f') {
				return a_char - 'a' + 10;
			}
			return -1;
		}

		std::vector<std::int16_t> _bytes;  // -1 = wildcard
	};
}
