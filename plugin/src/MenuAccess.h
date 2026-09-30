#pragma once

#include "logic/RowStacks.h"

namespace ScrapStacks::MenuAccess
{
	// One workbench list row's stacks, read live from the player's inventory.
	struct Row
	{
		RE::TESBoundObject*   object{ nullptr };
		std::vector<RowStack> stacks;
		// Mods on the copy the yield is computed from, and on the first stack in the
		// row that differs from it (empty if none), described for the log.
		std::string scrappedMods;
		std::string otherMods;
		// A favorited copy is never stack-scrapped; vanilla's own single-copy removal
		// could pick it, so any favorite in the row disables stack scrap for it.
		bool hasFavorite{ false };
	};

	[[nodiscard]] std::uint32_t RowCount(RE::ExamineMenu* a_menu);

	[[nodiscard]] std::optional<Row> ReadRow(RE::ExamineMenu* a_menu, std::int32_t a_rowIndex);

	// The live list of scrap results (item, count) that both the confirm dialog and
	// the grant in ScrapItemAccepted read.
	[[nodiscard]] RE::BSTArray<RE::BSTTuple<RE::TESBoundObject*, std::uint32_t>>& ScrappingArray(RE::ExamineMenu* a_menu);

	// The selected row as the player sees it. The engine's selectedIndex is a position in
	// its own entry order, but FallUI sorts the list for display and translates between
	// the two, so "same engine index" is not "same place on screen". These read and
	// write the on-screen position: FallUI's selectedIndexModNoShift when present,
	// otherwise the plain selectedIndex (vanilla, where the two orders are the same).
	[[nodiscard]] std::optional<std::int32_t> DisplayRow(RE::ExamineMenu* a_menu);
	[[nodiscard]] std::uint32_t               DisplayRowCount(RE::ExamineMenu* a_menu);

	// Selects on-screen row a_row, moved to the nearest row the list's filter shows.
	// Returns the row actually selected, or nullopt if Scaleform refused.
	std::optional<std::int32_t> SelectDisplayRow(RE::ExamineMenu* a_menu, std::int32_t a_row);

	// How far the list is scrolled, in rows. Restoring it after re-selecting keeps
	// the list still on screen; otherwise the list scrolls just enough to show the
	// selection, which parks it at the top or bottom edge.
	[[nodiscard]] std::optional<std::int32_t> ScrollPosition(RE::ExamineMenu* a_menu);
	void                                      RestoreScrollPosition(RE::ExamineMenu* a_menu, std::int32_t a_position);

	// Removes a_count copies from the player's stack at a_stackIndex of a_object.
	void RemoveFromStack(RE::TESBoundObject* a_object, std::uint32_t a_stackIndex, std::uint32_t a_count);

	// Finds the current index of the stack object at a_identity in a_object's stack
	// list, or nullopt if it no longer exists.
	[[nodiscard]] std::optional<std::uint32_t> FindStackIndex(RE::TESBoundObject* a_object, std::uintptr_t a_identity);
}
