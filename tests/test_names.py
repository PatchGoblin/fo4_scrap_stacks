from tools.names import parse_ids_header, parse_vtable_header

IDS_SNIPPET = """
namespace RE::ID
{
	namespace ExamineMenu
	{
		inline constexpr REL::VariantID BuildWeaponScrappingArray{ 646841, 2223077 };
		inline constexpr REL::VariantID GetBuildConfirmQuestion{ 1360189, 2223057 }; // Check
	}

	namespace ExteriorCellSingleton
	{
		inline constexpr REL::VariantID Singleton{ 128691, 2689084, 4796370 };
	}
	namespace BGSInventoryList
	{
		inline constexpr REL::ID RemoveItem1{ 555 };
	}
}
"""

VTABLE_SNIPPET = """
namespace RE::VTABLE
{
		inline constexpr std::array<REL::ID, 2>  ExamineMenu{ REL::ID(1032946), REL::ID(1005382) };
		inline constexpr std::array<REL::ID, 1>  __ScrapItemCallback{ REL::ID(1510330) };
}
"""


def test_ids_header_takes_the_old_gen_id_and_qualifies_with_namespace():
    names = parse_ids_header(IDS_SNIPPET)

    assert names[646841] == "ExamineMenu::BuildWeaponScrappingArray"
    assert names[1360189] == "ExamineMenu::GetBuildConfirmQuestion"
    assert names[128691] == "ExteriorCellSingleton::Singleton"
    assert names[555] == "BGSInventoryList::RemoveItem1"
    assert 2223077 not in names


def test_vtable_header_numbers_secondary_vtables():
    names = parse_vtable_header(VTABLE_SNIPPET)

    assert names[1032946] == "vtbl ExamineMenu"
    assert names[1005382] == "vtbl ExamineMenu[1]"
    assert names[1510330] == "vtbl __ScrapItemCallback"


RUNTIME_SNIPPET = """
namespace RE::ID
{
	namespace ExamineMenu
	{
		inline constexpr REL::VariantID BuildWeaponScrappingArray{ 646841, 2223077 };
	}
	namespace ExteriorCellSingleton
	{
		inline constexpr REL::VariantID Singleton{ 128691, 2689084, 4796370 };
	}
}
"""


def test_ids_header_picks_the_variant_for_the_runtime():
    ng = parse_ids_header(RUNTIME_SNIPPET, runtime=1)
    ae = parse_ids_header(RUNTIME_SNIPPET, runtime=2)

    assert ng[2689084] == "ExteriorCellSingleton::Singleton"
    assert ae[4796370] == "ExteriorCellSingleton::Singleton"
    # A missing trailing variant repeats the last one, as REL::VariantID does.
    assert ae[2223077] == "ExamineMenu::BuildWeaponScrappingArray"
    assert 646841 not in ae
