#pragma once

#include "logic/Discovery.h"

namespace ScrapStacks::GameDiscovery
{
	// Runs discovery (logic/Discovery.h) over the running Fallout4.exe, cross-checked
	// against the installed Address Library database. Logs every failed check and
	// returns nothing if any failed, so the caller installs no hooks.
	[[nodiscard]] std::optional<HookSites> Run();

	// Where the running Fallout4.exe is loaded; hook sites are RVAs from here.
	[[nodiscard]] std::uintptr_t ImageBase();
}
