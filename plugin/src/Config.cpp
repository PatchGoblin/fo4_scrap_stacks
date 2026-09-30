#include "PCH.h"

#include "Config.h"

namespace ScrapStacks
{
	namespace
	{
		constexpr auto kIniPath = "Data/F4SE/Plugins/ScrapStacks.ini"sv;

		std::string_view Trim(std::string_view a_text)
		{
			const auto first = a_text.find_first_not_of(" \t\r\n"sv);
			if (first == std::string_view::npos) {
				return {};
			}
			const auto last = a_text.find_last_not_of(" \t\r\n"sv);
			return a_text.substr(first, last - first + 1);
		}

		bool AsBool(std::string_view a_value)
		{
			return a_value == "1"sv || a_value == "true"sv || a_value == "True"sv;
		}
	}

	Config& Config::Get()
	{
		static Config instance;
		return instance;
	}

	void Config::Load()
	{
		std::ifstream file{ std::filesystem::path{ kIniPath } };
		if (!file) {
			REX::INFO("no ScrapStacks.ini found, using defaults");
			return;
		}

		std::string line;
		while (std::getline(file, line)) {
			const auto trimmed = Trim(line);
			if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#' ||
				trimmed.front() == '[') {
				continue;
			}

			const auto split = trimmed.find('=');
			if (split == std::string_view::npos) {
				continue;
			}
			const auto key = Trim(trimmed.substr(0, split));
			const auto value = Trim(trimmed.substr(split + 1));

			if (key == "Diagnostics"sv) {
				diagnostics = AsBool(value);
			} else if (key == "StackModifierKey"sv) {
				try {
					stackModifierKey = std::stoi(std::string{ value }, nullptr, 0);
				} catch (const std::exception&) {
					REX::WARN("ignoring malformed StackModifierKey {}", value);
				}
			} else {
				REX::WARN("ignoring unknown ScrapStacks.ini key {}", key);
			}
		}

		REX::INFO("diagnostics: {}, stack modifier key: {:#x}", diagnostics, stackModifierKey);
	}
}
