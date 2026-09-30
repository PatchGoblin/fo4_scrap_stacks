#pragma once

#include "logic/Discovery.h"

namespace ScrapStacks::ScrapHooks
{
	// Installs the workbench scrap hooks at the sites discovery found in the image
	// loaded at a_base.
	void Install(const HookSites& a_sites, std::uintptr_t a_base);
}
