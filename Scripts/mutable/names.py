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
PARAM_BODY = "Body"
PARAM_FIT_BASE = "Fit Base"
FIT_SUFFIX, NO_FIT, PARAM_FIT_STRENGTH = " Fit", "None", "Fit Strength"  # per slot: which Fit_<source> morph of Fit Base
OWN = f"{MUTABLE_ROOT}/Bases/SK_MutableOwn_{{sex}}"  # .format(sex="Male"): RamilMH's / BikaMH's body, the Own build
BASE = f"{MUTABLE_ROOT}/Bases/SK_MutableBase_{{sex}}"  # .format(sex="Male")
# Fit morphs: morph target "Fit_<Source>" on each sex's base moves it onto the body a wardrobe source was made on
# (setup_mutable.py stage fits); a piece is reshaped from it.
FIT_PREFIX = "Fit_"
# Each source's body per sex: "dna:<type>" = a MetaHuman compatibility body (the MetaHuman plugin's FixedCompatibility
# folder); "pose:<mesh>" = a skeletal mesh on another skeleton, posed like the base first (its pieces too:
# UShinobiMutableMeshLibrary::ReadPosedSurface / RebindPiece); else a MetaHuman skeletal mesh. The City Sample crowd was
# made on the tal_nrw bodies; the Mutable Sample's MetaHuman clothing on its female MetaHuman (female only), its Cyborg
# pieces on its male body; the Fab leather jacket on the UE mannequins.
FIT_SOURCES = {
    "CitySample": {"Male": "dna:m_tal_nrw", "Female": "dna:f_tal_nrw"},
    "MHSample": {"Female": "/Game/MetaHumans/Body/SKM_MH_Female_BodyMesh"},
    "Cyborg": {"Male": "pose:/Game/Character/Body/SK_BaseBody", "Female": "pose:/Game/Character/Body/SK_BaseBody"},
    "Mannequin": {"Male": "pose:/Game/Jacket/Meshes/SKM_Manny", "Female": "pose:/Game/Jacket/Meshes/SKM_Quinn"},
}
# Wardrobe pieces from those sources, appended after the City Sample rows: (row, slot, source, {sex: mesh}). Pieces of a
# "pose:" source are rebound onto each sex's base (/Game/Shinobi/Mutable/Wardrobe/<Source>/SK_<Row>_<Sex>).
SAMPLE_CONTENT = "E:/MutableSample/Content"  # the Mutable Sample project (Fab), copied into Content/ by stage samplecopy
SAMPLE_FOLDERS = ["Character", "MetaHumans", "Shared"]
_C, _MH = "/Game/Character", "/Game/MetaHumans/Clothing"
_BOTH = lambda mesh: {"Male": mesh, "Female": mesh}  # noqa: E731
_FEMALE = lambda mesh: {"Female": mesh}  # noqa: E731  (women's cuts: a male fit can't take their shape)
SAMPLE_PIECES = [
    ("CyborgJacketA", "Top", "Cyborg", _BOTH(f"{_C}/Jacket_A/SK_Jacket_A")),
    ("CyborgJacketB", "Top", "Cyborg", _BOTH(f"{_C}/Jacket_B/SK_Jacket_B")),
    ("CyborgTShirt", "Top", "Cyborg", _BOTH(f"{_C}/TShirt/SK_TShirt")),
    ("CyborgTankTop", "Top", "Cyborg", _BOTH(f"{_C}/TankTop/SK_TankTop")),
    ("MHSampleLongShirt", "Top", "MHSample", _FEMALE(f"{_MH}/Shirts/LongShirt/MH_Female_Outfits_2")),
    ("MHSampleLongShirt2", "Top", "MHSample", _FEMALE(f"{_MH}/Shirts/LongShirt_2/MH_Female_Outfits_2")),
    ("MHSampleShortShirt", "Top", "MHSample", _FEMALE(f"{_MH}/Shirts/ShortShirt/MH_Female_Outfits_2")),
    ("CyborgPants", "Bottom", "Cyborg", _BOTH(f"{_C}/Pants/SK_Pants")),
    ("CyborgScifiPants", "Bottom", "Cyborg", _BOTH(f"{_C}/ScifiPants/SK_ScifiPants")),
    ("MHSampleCargoPants", "Bottom", "MHSample", _FEMALE(f"{_MH}/CargoPants/MH_Female_Outfits")),
    ("MHSampleLongJeans", "Bottom", "MHSample", _FEMALE(f"{_MH}/Jeans/LongJeans/MH_Female_Outfits")),
    ("MHSampleShortJeans", "Bottom", "MHSample", _FEMALE(f"{_MH}/Jeans/ShortJeans/MH_Female_Outfits")),
    ("CyborgBoots", "Shoes", "Cyborg", _BOTH(f"{_C}/Boots/SK_Boots")),
    ("CyborgShoes", "Shoes", "Cyborg", _BOTH(f"{_C}/Shoes/SK_Shoes")),
    ("CyborgSneakers", "Shoes", "Cyborg", _BOTH(f"{_C}/Sneakers/SK_Shoes")),
    ("MHSampleBoots", "Shoes", "MHSample", _FEMALE(f"{_MH}/Boots/MH_Female_Outfits_3")),
    ("MHSampleShoes", "Shoes", "MHSample", _FEMALE(f"{_MH}/Shoes/MH_Female_Outfits_3")),
    ("MannequinLeatherJacket", "Outer", "Mannequin", {"Male": "/Game/Jacket/Meshes/SKM_Manny_Jacket", "Female": "/Game/Jacket/Meshes/SKM_Quinn_Jacket"}),
]
FIT_MAX_MOVE, FIT_SMOOTH = 12.0, 2  # cm (broad mannequin shoulders lie ~10 cm off a MetaHuman), passes (UShinobiMutableMeshLibrary::BakeFitMorph)
# Left where the base has them (with their children), faded out over FIT_KEEP_FADE rings: the bodies' hands differ in
# pose and their necks end differently, so projecting there shears triangles, which Reshape turns into spikes.
FIT_KEEP_BONES, FIT_KEEP_FADE = ["hand_l", "hand_r", "neck_02"], 12
PARAM_BUILD = "Build"
BUILDS = ["Under", "Normal", "Over", "Own"]  # EShinobiBuild order; Normal has no morph
PARAM_BUILD_BLEND = "Build Blend"
PARAM_SKIN_TONE = "Skin Tone"
PARAM_AGED = "Aged"
PARAM_MAKE_UP = "Make Up"
PARAM_FRECKLES = "Freckles"
SLOTS = ["Top", "Bottom", "Shoes", "Outer", "Accessory", "Cloak"]  # EShinobiWardrobeSlot order
MESH_SUFFIX = " Mesh"
CLIP_SUFFIX = " Clip"
HIDES_SUFFIX = " Hides"
ON, OFF = "ON", "OFF"
WARDROBE_TABLE = f"{MUTABLE_ROOT}/DT_Wardrobe_{{slot}}"  # .format(slot="Top")
# Coverage volumes (setup_mutable.py stage coverage; UShinobiMutableMeshLibrary::MakeClipVolume).
CLIP = f"{MUTABLE_ROOT}/Wardrobe/Clips/SK_Clip_{{slot}}_{{row}}_{{sex}}"
CLIP_REACH, CLIP_ERODE, CLIP_OUT, CLIP_DEPTH = 4.0, 1, 0.3, 3.0  # cm, rings, cm, cm
MAX_TATTOOS = 3

# Roster characters with a grafted static head, and their base sex (catalog Heads order = this order).
HEADS = {"Hero": "Male", "Bika": "Female", "Cat": "Female", "Misha": "Male", "Pabu": "Male", "Ramil": "Male", "Elvin": "Male"}
HEADS_DIR = f"{MUTABLE_ROOT}/Heads"
