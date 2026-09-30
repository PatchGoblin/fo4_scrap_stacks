#pragma once

namespace ScrapStacks
{
	// Everything the plugin reads from ScrapStacks.ini. Defaults are usable as-is, so
	// a missing ini is not an error.
	struct Config
	{
		// Log the workbench list state around every scrap, so the selection-jump
		// behaviour can be diagnosed from a log instead of guessed at.
		bool diagnostics{ false };

		// Hold this key (a Windows virtual-key code) while pressing Scrap to scrap every
		// unequipped copy in the selected row. 0x10 is Shift.
		int stackModifierKey{ 0x10 };

		static Config& Get();

		// Reads Data/F4SE/Plugins/ScrapStacks.ini if present. Never throws.
		void Load();
	};
}
