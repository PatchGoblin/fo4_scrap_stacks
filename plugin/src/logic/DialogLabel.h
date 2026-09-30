#pragma once

#include <cstdint>
#include <format>
#include <string>

namespace ScrapStacks
{
	// What a Scrap press with the stack modifier held turned into, so the confirm
	// dialog can say so rather than a stack scrap silently taking fewer copies.
	struct StackDecision
	{
		enum class Kind
		{
			kNone,       // plain scrap, or a genuine single copy: nothing to explain
			kStack,      // stack scrap armed for `copies` of the row's `rowCopies`
			kFavorite,   // refused: a copy in the row is favorited
			kMixedMods,  // refused: the copies' mods don't line up with the one shown
		};

		Kind          kind{ Kind::kNone };
		std::uint32_t copies{ 0 };
		std::uint32_t rowCopies{ 0 };
	};

	// Appended to the item name in the scrap confirm dialog.
	[[nodiscard]] inline std::string LabelSuffix(const StackDecision& a_decision)
	{
		using Kind = StackDecision::Kind;
		switch (a_decision.kind) {
		case Kind::kStack:
			return a_decision.copies == a_decision.rowCopies ?
			           std::format(" (x{})", a_decision.copies) :
			           std::format(" (x{} of {}, rest have other mods)", a_decision.copies, a_decision.rowCopies);
		case Kind::kFavorite:
			return " (x1, stack has a favorite)";
		case Kind::kMixedMods:
			return " (x1, copies have different mods)";
		case Kind::kNone:
		default:
			return {};
		}
	}
}
