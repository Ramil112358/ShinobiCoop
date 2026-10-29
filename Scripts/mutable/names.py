"""Paths and names shared by the Mutable setup scripts (Scripts/setup_mutable.py, Scripts/mutable/*)."""

MUTABLE_ROOT = "/Game/Shinobi/Mutable"
CATALOG = "/Game/Shinobi/Characters/Creator/DA_CreatorCatalog"

# The MetaHuman body template every MetaHuman Creator body is fitted from (engine content).
BODY_TEMPLATE = "/MetaHumanCharacter/Body/IdentityTemplate/SKM_Body"

_MH = "/Game/Shinobi/MetaHumans"
_SOURCE = "/MetaHumanCharacter/Optional/Clothing/DefaultGarment/SourceBodies"
# Full MetaHuman bodies in the project and engine (the City Sample crowd "body" meshes are only the visible skin).
FULL_BODIES = {
    "RamilMH": f"{_MH}/RamilMH/Build/MHC_RamilMH/Body/SKM_MHC_RamilMH_BodyMesh",
    "BikaMH": f"{_MH}/BikaMH/Build/MHC_BikaMH/Body/SKM_MHC_BikaMH_BodyMesh",
    **{f"CF_{s}{i}": f"{_MH}/Creator/Build/MHC_CF_{s}{i}/Body/SKM_MHC_CF_{s}{i}_BodyMesh" for s in "MF" for i in range(1, 5)},
    "archetype": f"{_SOURCE}/archetype_SkelMesh",
    "f_tall_ovw": f"{_SOURCE}/f_tall_ovw_SkelMesh",
    "m_srt_unw": f"{_SOURCE}/m_srt_unw_SkelMesh",
}

# Their own MetaHuman bodies (AShinobiPlayerCharacter::Roster): the "Own" build of each sex's base.
OWN_BODIES = {"Male": FULL_BODIES["RamilMH"], "Female": FULL_BODIES["BikaMH"]}

# Mutable parameter names: mirror Source/ShinobiCoop/Shinobi/Hero/ShinobiMutableNames.h exactly.
OBJECT = f"{MUTABLE_ROOT}/CO_Shinobi"
PALETTE = f"{MUTABLE_ROOT}/DT_Palette"
PARAM_SEX = "Sex"
PARAM_BUILD = "Build"
PARAM_BUILD_BLEND = "Build Blend"
PARAM_SKIN_TONE = "Skin Tone"
PARAM_AGED = "Aged"
PARAM_MAKE_UP = "Make Up"
PARAM_FRECKLES = "Freckles"
SLOTS = ["Top", "Bottom", "Shoes", "Outer", "Accessory", "Cloak"]  # EShinobiWardrobeSlot order
COLOUR_SUFFIX = " Colour"
WEAR_SUFFIX = " Wear"
MAX_TATTOOS = 3
