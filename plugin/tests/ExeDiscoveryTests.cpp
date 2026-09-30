// Runs the plugin's startup discovery against every Fallout4.exe on disk.
//
// This is the assurance behind "works on any version": the exact code the plugin
// runs in game (logic/Discovery.h) is pointed at each unpacked exe in re/, with the
// same Address Library cross-checks the plugin makes, and must find the same hook
// sites an independent analysis found (tools/, Python). Builds that aren't on disk
// are skipped; see tools/fetch_exes.ps1 to get them.

#include <doctest/doctest.h>

#include "logic/AddressLibrary.h"
#include "logic/Discovery.h"
#include "logic/PeImage.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace ScrapStacks;

namespace
{
	struct Expected
	{
		const char*   version;
		std::uint32_t examineMenuVtbl;
		std::uint32_t scrapCallbackVtbl;
		std::uint32_t yieldCall;
		std::uint32_t labelCall;
		std::uint32_t removalCall;
	};

	// From the Python analysis (tools/rtti.py and the site survey), not from this code.
	constexpr Expected kExpected[]{
		{ "1.10.163", 0x2D1C188, 0x2D1C5A8, 0xB18DE9, 0xB18E67, 0xB19278 },
		{ "1.10.984", 0x2342A20, 0x2342CA8, 0xA0B166, 0xA0B1E4, 0xA0B5C9 },
		{ "1.11.137", 0x2526330, 0x25265B8, 0xA5BD38, 0xA5BDB6, 0xA5C1A9 },
		{ "1.11.159", 0x2527330, 0x25275B8, 0xA5BE78, 0xA5BEF6, 0xA5C2E9 },
		{ "1.11.169", 0x2529320, 0x25295A8, 0xA5C268, 0xA5C2E6, 0xA5C6D9 },
		{ "1.11.191", 0x252E300, 0x252E588, 0xA5EC98, 0xA5ED16, 0xA5F109 },
		{ "1.11.221", 0x252E300, 0x252E588, 0xA5ECD8, 0xA5ED56, 0xA5F149 },
		{ "1.11.240", 0x2536370, 0x25365F8, 0xA5EFE8, 0xA5F066, 0xA5F459 },
	};

	std::vector<std::uint8_t> ReadFile(const std::filesystem::path& a_path)
	{
		std::ifstream file{ a_path, std::ios::binary };
		return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	}

	// "Fallout4_1_11_191.exe.unpacked.exe" -> "1.11.191"
	std::string VersionOf(const std::filesystem::path& a_exe)
	{
		auto name = a_exe.filename().string();
		name = name.substr(std::string_view{ "Fallout4_" }.size());
		name = name.substr(0, name.find(".exe"));
		std::ranges::replace(name, '_', '.');
		return name;
	}

	std::vector<std::filesystem::path> UnpackedExes()
	{
		std::vector<std::filesystem::path> exes;
		const std::filesystem::path dir{ SCRAPSTACKS_RE_DIR };
		if (!std::filesystem::exists(dir)) {
			return exes;
		}
		for (const auto& entry : std::filesystem::directory_iterator(dir)) {
			if (entry.path().filename().string().ends_with(".exe.unpacked.exe")) {
				exes.push_back(entry.path());
			}
		}
		std::ranges::sort(exes);
		return exes;
	}

	// What the plugin gets from CommonLib's Address Library IDs at runtime, read here
	// from the same database file.
	std::optional<AddressHints> HintsFor(const std::string& a_version)
	{
		auto dashed = a_version;
		std::ranges::replace(dashed, '.', '-');  // 1.11.191 -> version-1-11-191-0.bin
		const auto path = std::filesystem::path{ SCRAPSTACKS_ADDRLIB_DIR } / ("version-" + dashed + "-0.bin");
		const auto library = AddressLibrary::Parse(ReadFile(path));
		if (!library) {
			return std::nullopt;
		}
		return HintsFromLibrary(*library, IsOldGen(a_version));
	}

	std::filesystem::path ThisExe()
	{
		std::wstring buffer(MAX_PATH, L'\0');
		buffer.resize(GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size())));
		return buffer;
	}
}

TEST_CASE("discovery finds the hook sites on every Fallout4.exe on disk")
{
	const auto exes = UnpackedExes();
	if (exes.empty()) {
		MESSAGE("no unpacked exes in " SCRAPSTACKS_RE_DIR "; run tools/fetch_exes.ps1 to test against real builds");
		return;
	}

	for (const auto& exe : exes) {
		const auto version = VersionOf(exe);
		CAPTURE(version);

		const auto bytes = ReadFile(exe);
		const auto image = PeImage::Parse(bytes, PeImage::Layout::kFile);
		REQUIRE(image.has_value());

		const auto hints = HintsFor(version);
		CHECK_MESSAGE(hints.has_value(), "no Address Library database for this build");

		const auto result = Discover(*image, hints.value_or(AddressHints{}));
		for (const auto& failure : result.failures) {
			MESSAGE(failure);
		}
		REQUIRE(result.sites.has_value());
		const auto& sites = *result.sites;

		const auto expected = std::ranges::find(kExpected, version, &Expected::version);
		if (expected == std::end(kExpected)) {
			MESSAGE("build " << version << " is not in the expected table; discovery succeeded on its own checks");
			continue;
		}
		CHECK(sites.examineMenuVtbl == expected->examineMenuVtbl);
		CHECK(sites.scrapCallbackVtbl == expected->scrapCallbackVtbl);
		CHECK(sites.yieldCall == expected->yieldCall);
		CHECK(sites.labelCall == expected->labelCall);
		CHECK(sites.removalCall == expected->removalCall);
	}
	MESSAGE("checked " << exes.size() << " build(s)");
}

TEST_CASE("discovery refuses an image that isn't Fallout 4")
{
	// Any valid PE without the menu's RTTI: the test binary itself.
	const auto bytes = ReadFile(ThisExe());
	const auto image = PeImage::Parse(bytes, PeImage::Layout::kFile);
	REQUIRE(image.has_value());

	const auto result = Discover(*image, AddressHints{});

	CHECK_FALSE(result.sites.has_value());
	CHECK_FALSE(result.failures.empty());
}

TEST_CASE("old-gen and next-gen builds are told apart by version")
{
	CHECK(IsOldGen("1.10.163"));
	CHECK_FALSE(IsOldGen("1.10.984"));
	CHECK_FALSE(IsOldGen("1.11.240"));
	CHECK_FALSE(IsOldGen("1-10-163"));  // unparseable is not old-gen
}
