#include "PCH.h"

#include "GameDiscovery.h"

#include "logic/AddressLibrary.h"
#include "logic/PeImage.h"

namespace ScrapStacks::GameDiscovery
{
	namespace
	{
		std::string GameVersion()
		{
			const auto version = REX::FModule::GetExecutingModule().GetFileVersion();
			return std::format("{}.{}.{}", version.major(), version.minor(), version.patch());
		}

		// The loaded image spans SizeOfImage bytes from its base (PE optional header).
		std::span<const std::uint8_t> LoadedImage(std::uintptr_t a_base)
		{
			std::uint32_t ntOffset;
			std::memcpy(&ntOffset, reinterpret_cast<const void*>(a_base + 0x3C), sizeof(ntOffset));
			std::uint32_t sizeOfImage;
			std::memcpy(&sizeOfImage, reinterpret_cast<const void*>(a_base + ntOffset + 24 + 56), sizeof(sizeOfImage));
			return { reinterpret_cast<const std::uint8_t*>(a_base), sizeOfImage };
		}

		// Same file CommonLib loads: Data/F4SE/Plugins/version-<a>-<b>-<c>-<d>.bin.
		std::optional<AddressLibrary> LoadAddressLibrary()
		{
			const auto version = REX::FModule::GetExecutingModule().GetFileVersion();
			const std::filesystem::path path{ std::format("Data/F4SE/Plugins/version-{}.bin", version.string("-")) };
			std::ifstream file{ path, std::ios::binary };
			if (!file) {
				REX::ERROR("discovery: could not open {}", path.string());
				return std::nullopt;
			}
			std::vector<std::uint8_t> bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
			auto library = AddressLibrary::Parse(std::move(bytes));
			if (!library) {
				REX::ERROR("discovery: {} is not a valid Address Library database", path.string());
			}
			return library;
		}
	}

	std::uintptr_t ImageBase()
	{
		return REX::FModule::GetExecutingModule().GetBaseAddress();
	}

	std::optional<HookSites> Run()
	{
		const auto version = GameVersion();
		const auto base = ImageBase();
		const auto image = PeImage::Parse(LoadedImage(base), PeImage::Layout::kMapped, base);
		if (!image) {
			REX::ERROR("discovery: could not read the Fallout4.exe image headers");
			return std::nullopt;
		}

		const auto library = LoadAddressLibrary();
		const auto hints = library ? HintsFromLibrary(*library, IsOldGen(version)) : AddressHints{};

		const auto started = std::chrono::steady_clock::now();
		const auto result = Discover(*image, hints);
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
		if (!result.sites) {
			REX::ERROR("discovery on Fallout 4 {}: {} check(s) failed, installing no hooks. "
			           "The game runs exactly as if this mod were not installed. "
			           "If another mod changes the workbench scrap code, it may have patched these spots first.",
				version, result.failures.size());
			for (const auto& failure : result.failures) {
				REX::ERROR("  {}", failure);
			}
			return std::nullopt;
		}

		const auto& sites = *result.sites;
		REX::INFO("discovery on Fallout 4 {}: all checks passed in {} ms", version, elapsed.count());
		REX::INFO("  ExamineMenu vtable {:#x}, scrap callback vtable {:#x}", sites.examineMenuVtbl, sites.scrapCallbackVtbl);
		REX::INFO("  yield call {:#x}, dialog label call {:#x}, copy removal call {:#x}", sites.yieldCall, sites.labelCall, sites.removalCall);
		return sites;
	}
}
