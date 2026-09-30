#include "PCH.h"

#include "Config.h"
#include "GameDiscovery.h"
#include "ScrapHooks.h"

namespace
{
	constexpr auto kPluginName = "ScrapStacks"sv;
	constexpr std::uint32_t kVersionMajor{ 0 };
	constexpr std::uint32_t kVersionMinor{ 1 };
	constexpr std::uint32_t kVersionPatch{ 0 };
}

// OG F4SE (0.6.23) consumes F4SEPlugin_Query and ignores PluginVersionData, while
// NG/AE consume F4SEPlugin_Version and ignore Query. Exporting both lets the DLL
// load everywhere; discovery in F4SEPlugin_Load decides whether it acts.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(
	const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	if (!a_f4se || !a_info) {
		return false;
	}
	if (a_f4se->RuntimeVersion() != F4SE::RUNTIME_1_10_163) {
		return false;  // newer runtimes go through F4SEPlugin_Version
	}
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = kPluginName.data();
	a_info->version = kVersionMajor;
	return true;
}

extern "C" DLLEXPORT constinit auto F4SEPlugin_Version = []() noexcept {
	F4SE::PluginVersionData data{};

	data.PluginVersion({ kVersionMajor, kVersionMinor, kVersionPatch, 0 });
	data.PluginName(kPluginName.data());
	data.AuthorName("ScrapStacks");
	data.UsesSigScanning(true);
	data.HasNoStructUse(false);
	data.UsesAddressLibrary(true);
	data.IsLayoutDependent(true);
	// commonlibf4's helpers set only the AE bit; advertise NG support as well.
	data.addressIndependence |= (1u << 1);
	data.structureIndependence |= (1u << 1);
	data.CompatibleVersions({});

	return data;
}();

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	// Three 5-byte call hooks need a 14-byte trampoline branch each.
	F4SE::Init(a_f4se, { .trampoline = true, .trampolineSize = 64 });

	REX::INFO("ScrapStacks v{}.{}.{}", kVersionMajor, kVersionMinor, kVersionPatch);
	REX::INFO("runtime: {}", a_f4se->RuntimeVersion().string());

	ScrapStacks::Config::Get().Load();

	// Discovery checks everything the hooks rely on against this exact game build.
	// If anything is off, no hooks go in and the game runs as if the mod weren't
	// installed; returning true keeps F4SE from reporting a load failure.
	if (const auto sites = ScrapStacks::GameDiscovery::Run()) {
		ScrapStacks::ScrapHooks::Install(*sites, ScrapStacks::GameDiscovery::ImageBase());
	}
	return true;
}
