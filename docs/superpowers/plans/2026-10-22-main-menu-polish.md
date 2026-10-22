# Main Menu Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the plain main menu with a tabbed, styled "Live Stage" menu: PLAY (map cards + server browser), SHINOBI (roster portraits + outfit, live 3D character), JUTSU (loadout with descriptions), SETTINGS.

**Architecture:** Menus stay code-built UMG. A shared style kit (`ShinobiMenuStyle`) restyles every menu panel. A new `AShinobiMenuStage` actor in `Lvl_MainMenu` holds a hidden Manny playing `MM_Idle`, a `UShinobiBodyComponent` wearing the pick, and an outfit component dressed by the same code the pawn uses. `UShinobiMainMenuWidget` becomes a tab shell (tab bar + `UWidgetSwitcher`). It embeds the existing Settings and Skills widgets in menu modes. Portraits and map images are PNGs captured in-game by a world subsystem behind command-line flags, then imported by a Python script.

**Tech Stack:** UE 5.8 C++ (UMG built in `RebuildWidget`), Unreal automation tests (`Scripts/run_tests.ps1`), Unreal Python (`Scripts/run_python.ps1`), PowerShell launch scripts.

**Spec:** `docs/superpowers/specs/2026-10-20-main-menu-polish-design.md`

## Global Constraints

- **Branch `feature/main-menu` off `main`.** Never `git checkout`/`switch` in the shared tree. Its uncommitted RamilMH work stays out of every commit. Commit through a separate index with `C:\Users\ahmad\AppData\Local\Temp\claude\e--ShinobiCoop\cbab4a06-5558-42a6-9d25-f252e4cab031\scratchpad\menu_commit.sh` (written in Task 1). That script commits whole files for paths RamilMH never touched. For files RamilMH did touch, it commits only the diff against a snapshot taken before this plan first edits them. RamilMH-touched files (`git diff main --stat`): `ShinobiPlayerCharacter.h/.cpp`, `ShinobiPlayerController.h/.cpp`, `Hero/ShinobiBodyComponent.h/.cpp`, `ShinobiCharacterBase.cpp`, `Tests/ShinobiHeroTests.cpp`, `Scripts/shot_chakra.ps1`, `Content/Shinobi/Characters/**`, `Content/Shinobi/Hero/**`, `Content/Shinobi/Outfits/**`.
- The code must compile both with and without the RamilMH changes. Never reference `FShinobiRosterEntry::MetaHuman` or `WearMetaHumanParts`.
- Uasset work (Lvl_MainMenu dressing, texture import) runs only with the GUI editor closed. Check the editor isn't running (`tasklist | grep -i UnrealEditor`) before any `run_python.ps1` call, and ask the user if it is.
- Unity build: anonymous-namespace names in UI `.cpp` files must be unique per file. Prefix them per file, e.g. `MenuStyle…`, `MainMenu…`.
- Accent `#ff7a1a` = `FLinearColor::FromSRGBColor(FColor(255,122,26))`. Panels are dark translucent. Headings are italic uppercase. Buttons and cards are parallelograms: `SetRenderTransformShear(FVector2D(-12.f, 0.f))` on the frame, `+12` on its content.
- Read the roster from `AShinobiPlayerCharacter::Roster[]` and the outfits from `Outfits[]`. Never hard-code character names.
- `GetMaps()` index order never changes, because `-ShinobiAutoHost*` flags and `HostGame(MapIndex, …)` depend on it.
- Settings and loadout save formats stay unchanged: `ShinobiSettings` keys `Character` / `Outfit`, and `ShinobiLoadout::Save`.
- Testing (user rule): run only the tests for what changed, e.g. `powershell -File Scripts/run_tests.ps1 -Filter ShinobiCoop.UI`. Run the full suite once, in Task 8.
- Build: `powershell -File Scripts/build.ps1` (editor closed, or Live Coding off).

## Review Focus

1. **A roster entry with no captured portrait** (a new character added later). The card shows the name on the accent gradient, with no blank or missing-texture checker. *Test in Task 2:* `ShinobiCoop.UI.MenuArtMissingIsNull`.
2. **An outfit with no copy fitted to the picked body.** The selector shows the outfit name with "(not built)", and the stage shows the bare body. *Test in Task 3:* `ShinobiCoop.UI.MenuStageMissingOutfitShowsBody`.
3. **An old or invalid saved loadout opened from the JUTSU tab.** The tab shows `Default()`, and an edit saves a loadout that `IsValid` accepts against the pawn's known jutsu. *Test in Task 6:* `ShinobiCoop.UI.MenuLoadoutSavesValid`.
4. **Gamepad-only use.** After every tab switch, keyboard focus lands on the new tab's first focusable widget, never on nothing. *Test in Task 4:* `ShinobiCoop.UI.MainMenuTabFocus`.
5. **A busy session** (hosting, joining or searching). HOST and Join rows are disabled and the message line shows the subsystem's message. Returning to the menu after a failed join shows the failure. *Covered by the existing `Refresh` path.* Task 4 keeps `SetIsEnabled(!IsBusy())` on every new action button. No new test.

---

### Task 1: Branch, commit helper, style kit

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/UI/ShinobiMenuStyle.h`, `ShinobiMenuStyle.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/UI/ShinobiMenuWidgets.cpp` (`AddButton`, `AddText` and the panel in `UShinobiMenuWidget::RebuildWidget` use the kit, so the pause menu and Settings get the look)
- Scratch: `…\scratchpad\menu_commit.sh`, `…\scratchpad\menu-base\` (snapshots)

**Interfaces — Produces:**
```cpp
namespace ShinobiMenuStyle
{
	SHINOBICOOP_API extern const FLinearColor Accent;      // #ff7a1a
	SHINOBICOOP_API extern const FLinearColor Panel;       // (0.02,0.02,0.04,0.78)
	SHINOBICOOP_API extern const FLinearColor TextDim;     // (0.7,0.7,0.75)
	SHINOBICOOP_API UTextBlock* Text(UWidgetTree& Tree, const FText& Text, int32 Size, const FLinearColor& Color, bool bHeading = false); // heading = italic bold caps
	SHINOBICOOP_API UButton* Button(UWidgetTree& Tree, const FText& Label, int32 Size, bool bPrimary = false); // sheared, accent on hover/focus; primary = filled accent, black text
	SHINOBICOOP_API UBorder* PanelBox(UWidgetTree& Tree, const FMargin& Padding); // translucent dark panel, accent left edge 3 px
	SHINOBICOOP_API FSlateBrush TextureBrush(UTexture2D* Texture, const FVector2D& Size); // like ShinobiHUDWidget's ArtBrush
}
```
`UShinobiIndexButton` stays. `ShinobiMenuStyle::Button` returns a plain `UButton`. `AddButton` builds a `UShinobiIndexButton` and applies the same style through `void ShinobiMenuStyle::StyleButton(UButton& Button, bool bPrimary)` (also exported).

- [ ] **Step 1: Create the branch and the commit helper.** Run `git branch feature/main-menu main`. Write `menu_commit.sh`. It takes `-m "<msg>"` and a path list and uses `GIT_INDEX_FILE=.git/menu.index`. It runs `git read-tree feature/main-menu`. For each path: if a snapshot `menu-base/<path>` exists, it pipes `git diff --no-index menu-base/<path> <path>` (rewritten to `a/<path> b/<path>`) into `git apply --cached`. Otherwise it runs `git update-index --add <path>`. Then it runs `write-tree`, then `commit-tree -p feature/main-menu`, then `update-ref refs/heads/feature/main-menu`. Add a `snap <path>` subcommand that copies the working file into `menu-base/<path>` if no snapshot exists yet. **Before any edit to a RamilMH-touched file, run `menu_commit.sh snap <path>`.**
- [ ] **Step 2: Implement `ShinobiMenuStyle`.**
  - Italic: set `FSlateFontInfo::TypefaceFontName` to `"BoldItalic"` on the engine Roboto, falling back to `"Bold"` if the composite font has no such face. Check with `Font.FontObject` typeface names.
  - Hover and focus use `FButtonStyle`: Normal `Panel`, Hovered/Pressed `Accent` at 35% alpha with an accent outline. Disabled is 40% opacity.
- [ ] **Step 3: Restyle the base.** Rewire `UShinobiMenuWidget::AddButton`, `AddText` and the panel to the kit. Use a title heading of size 34 with the accent underline.
- [ ] **Step 4: Build and run the related tests.** Run `Scripts/build.ps1`, then `Scripts/run_tests.ps1 -Filter ShinobiCoop.UI`. Expected: build OK, all UI tests pass (Settings and key bindings use these widgets).
- [ ] **Step 5: Commit** with `menu_commit.sh -m "feat: menu style kit; pause and settings menus restyled" <the 3 files>` and the design doc `docs/superpowers/specs/2026-10-20-main-menu-polish-design.md`.

### Task 2: Menu data — maps, art paths, settings setters, jutsu groups

**Files:**
- Modify: `Shinobi/Online/ShinobiSessionSubsystem.h/.cpp` (`FShinobiMap` fields, `GetMaps()` blurbs, helpers)
- Modify: `Shinobi/UI/ShinobiSettings.h/.cpp` (setters, `RowsFor`)
- Modify: `Shinobi/Abilities/ShinobiLoadout.h/.cpp` (groups)
- Create: `Shinobi/UI/ShinobiMenuArt.h/.cpp`
- Test: `Tests/ShinobiMenuTests.cpp` (new)

**Interfaces — Produces:**
```cpp
struct FShinobiMap { const TCHAR* DisplayName; const TCHAR* Path; bool bArena = false; const TCHAR* Blurb = TEXT(""); bool bTest = false; };
// UShinobiSessionSubsystem (static):
static TArray<int32> GetHostOrder();                      // co-op (not test), then arenas, then test; indices into GetMaps()
static TArray<EShinobiPvpFormat> GetModes(const FShinobiMap& Map); // co-op {None}; arena {OneVsOne, TwoVsTwo}
static int32 GetMaxPlayers(const FShinobiMap& Map, EShinobiPvpFormat Format); // co-op MaxPlayers(4); 1v1 2; 2v2 4
static int32 FindMapByName(const FString& DisplayName);   // INDEX_NONE if none (Find Games rows)
// ShinobiMenuArt:
FString MapId(const FShinobiMap& Map);          // Path basename minus "Lvl_": "London", "Arena_Camden", "ThirdPerson"
FString MapImagePath(const FShinobiMap& Map);   // "/Game/Shinobi/UI/Menu/T_Map_<Id>.T_Map_<Id>"
FString PortraitPath(int32 RosterIndex);        // "/Game/Shinobi/UI/Menu/T_Portrait_<Name>.T_Portrait_<Name>"
UTexture2D* LoadArt(const FString& Path);       // quiet load (LOAD_NoWarn|LOAD_Quiet); null when not captured yet
// ShinobiSettings:
void SetSavedCharacter(const FString& Name);    // ignores names not in Roster; saves like Cycle
void SetSavedOutfit(const FString& Name);       // ignores names not in Outfits
TArray<ERow> RowsFor(bool bMainMenu);           // main menu: every row except Character, Outfit
// ShinobiLoadout:
enum class EGroup : uint8 { Combat, Elements, SummonsAndDragons };
EGroup GroupOf(TSubclassOf<UShinobiJutsuAbility> Jutsu);
FText GroupName(EGroup Group);                  // "COMBAT", "ELEMENTS", "SUMMONS & DRAGONS"
```
`GroupOf`:
- Elements = EarthWall, WaterWall, WaterDragon, FireMeteor, IceMeteor.
- SummonsAndDragons = SummonPusho, SummonOwl, SummonMisha, SummonPabu, SummonDragon, SummonCrimsonDragon, Susanoo.
- Anything else is Combat.

Mark Test Level and Combat Level `bTest = true`. Write a one-line blurb for every map: setting, plus "co-op, up to 4" or "PvP arena".

- [ ] **Step 1: Write failing tests** in `Tests/ShinobiMenuTests.cpp` (flags `ShinobiTestFlags`, prefix `ShinobiCoop.UI.`):
  - `MenuHostOrder`: `GetHostOrder()` equals `{0,1,2,5,6,7,8,3,4}`, and every index appears once.
  - `MenuModes`: `GetModes(GetMaps()[1])` is `{None}`. `GetModes(GetMaps()[5])` is `{OneVsOne, TwoVsTwo}`. `GetMaxPlayers` returns 4 / 2 / 4.
  - `MenuArtPaths`: `MapId(GetMaps()[1]) == "London"`, `MapId(GetMaps()[6]) == "Arena_Camden"`, `MapId(GetMaps()[3]) == "ThirdPerson"`, `MapImagePath(GetMaps()[1]) == "/Game/Shinobi/UI/Menu/T_Map_London.T_Map_London"`, `PortraitPath(0) == "/Game/Shinobi/UI/Menu/T_Portrait_Hero.T_Portrait_Hero"`.
  - `MenuArtMissingIsNull`: `LoadArt("/Game/Shinobi/UI/Menu/T_Portrait_Nobody.T_Portrait_Nobody") == nullptr`, and no warning is logged (`AddExpectedError` not needed; assert the result only).
  - `MenuSettingsSetters`: save the current values. `SetSavedCharacter(Roster[1].Name)` makes `GetSavedCharacter()` return it. `SetSavedCharacter("Nobody")` leaves it unchanged. The same for the outfit. Restore the values, then `GConfig->Flush(false, GGameUserSettingsIni)` (memory gotcha: tests must flush their restore).
  - `MenuSettingsRows`: `RowsFor(true)` does not contain Character or Outfit and has `ERow::Num - 2` entries. `RowsFor(false)` has `ERow::Num`.
  - `MenuJutsuGroups`: `GroupOf(UGA_Fireball)` is Combat, `GroupOf(UGA_EarthWall)` is Elements, `GroupOf(UGA_Susanoo)` is SummonsAndDragons. Every jutsu in the pawn's known list (`GetDefault<AShinobiPlayerCharacter>()->GetKnownJutsu()`) gets a group.
- [ ] **Step 2: Run them and confirm they fail.** Run `Scripts/build.ps1` then `Scripts/run_tests.ps1 -Filter ShinobiCoop.UI.Menu`. Expected: a compile failure (missing symbols).
- [ ] **Step 3: Implement** the interfaces above. Rewire `UShinobiSettingsWidget` so it has a `bool bMainMenu` (public, set before `AddToViewport` or before embedding) and builds its entries from `RowsFor(bMainMenu)`. Its `OnEntry` maps through that list, and Back is omitted when `bMainMenu`.
- [ ] **Step 4: Run the tests.** Run `run_tests.ps1 -Filter ShinobiCoop.UI`. Expected: all pass, including the existing `SettingsCycleAndSave`.
- [ ] **Step 5: Commit** with `"feat: menu data: map blurbs and host order, art paths, settings setters, jutsu groups"`.

### Task 3: Menu stage actor and Lvl_MainMenu dressing

**Files:**
- Modify (snap first): `Shinobi/ShinobiPlayerCharacter.h/.cpp`. Extract the dressing from `ApplyOutfit()` into a static helper. `ApplyOutfit` keeps its gravity flag, `RefreshOutline` and the log line.
- Create: `Shinobi/UI/ShinobiMenuStage.h/.cpp`
- Create: `Scripts/setup_menu.py` (dresses `Lvl_MainMenu`)
- Test: `Tests/ShinobiMenuTests.cpp`

**Interfaces — Produces:**
```cpp
// AShinobiPlayerCharacter (public static):
/** Puts Outfits[OutfitIndex]'s copy fitted to Roster[BodyIndex] on Outfit, leader-posed to Body, and sets Body's HideMask; returns false (outfit hidden, body unmasked) when that copy isn't built. */
static bool DressOutfit(USkeletalMeshComponent& Body, USkeletalMeshComponent& Outfit, int32 OutfitIndex, int32 BodyIndex);
static bool IsOutfitBuilt(int32 OutfitIndex, int32 BodyIndex); // index 0 (None) = true; else the copy asset exists (quiet load)

UCLASS() class AShinobiMenuStage : public AActor
{
	void Show(int32 RosterIndex, int32 OutfitIndex); // falls back to Roster[0] if the pick isn't imported, like ApplyCharacter
	int32 GetShownBody() const;
	void SetFocus(EShinobiMenuStageFocus Focus);      // enum { Center, Left, Right }: eases the body sideways so tab panels don't cover it
	UCameraComponent* GetCamera() const;
	// components: Root, Driver (USkeletalMeshComponent, SKM_Manny_Simple, hidden, PlayAnimation(MM_Idle, true)),
	//             Body (UShinobiBodyComponent attached to Driver), Outfit (USkeletalMeshComponent attached to Body), Camera
};
```
Asset paths: `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple` and `/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle`. The body's retarget anim instance reads the driver as its attach parent, as on the pawn. Follow `UShinobiBodyComponent::HideDriver`.

- [ ] **Step 1: Write failing tests:**
  - `MenuStageShowsPick`: spawn `AShinobiMenuStage` in `FShinobiTestWorld` and call `Show(1, 0)`. If `Roster[1]` is imported, `Body->GetSkeletalMeshAsset()` is `Roster[1]`'s mesh and `GetShownBody() == 1`. Calling `Show(99, 0)` gives `GetShownBody() == 0`.
  - `MenuStageMissingOutfitShowsBody`: find a pair with `!IsOutfitBuilt(o, b)`, or skip with `AddInfo` if every pair is built. `Show(b, o)` leaves the outfit component invisible and the body visible.
  - `MenuStageWearsOutfit`: for the first pair where `IsOutfitBuilt` is true and the outfit index is above 0, `Show` sets the outfit mesh to the asset at `OutfitMeshPath` and makes its leader pose component the body.
- [ ] **Step 2: Build and confirm the new tests fail.**
- [ ] **Step 3: Implement `DressOutfit` / `IsOutfitBuilt`** by moving code out of `ApplyOutfit` (the mesh/mask lookup, the HideMask MIDs, the hips offset, the leader pose, the cloth teleport). `ApplyOutfit` calls it. Then implement `AShinobiMenuStage`.
- [ ] **Step 4: Run** `run_tests.ps1 -Filter ShinobiCoop.UI.Menu` and `-Filter ShinobiCoop.Outfit` (the outfit tests guard the extraction). Expected: all pass.
- [ ] **Step 5: Dress the level.**
  - First inspect `Lvl_MainMenu` through a read-only `run_python.ps1` script that lists actors.
  - Write `Scripts/setup_menu.py`, which is idempotent: it deletes and re-places actors tagged `ShinobiMenu`. It places one `AShinobiMenuStage` and a dusk backdrop: sky/directional light at low sun angle with an orange tint, a key light and a rim light on the stage, and backdrop meshes from committed content only (London/Fab city props). Keep the stage on screen-left third by default.
  - Run it with `run_python.ps1 -Script Scripts/setup_menu.py` (editor closed).
- [ ] **Step 6: Commit** the two source pairs, the tests, `setup_menu.py` and `Content/Shinobi/Maps/Lvl_MainMenu.umap`, with `"feat: menu stage: the pick idles in its outfit in Lvl_MainMenu"`.

### Task 4: Tab shell, PLAY tab (Host + Find Games), SETTINGS tab

**Files:**
- Modify: `Shinobi/UI/ShinobiMenuWidgets.h/.cpp`. `UShinobiMainMenuWidget` is rewritten. The base class stays for the pause menu and Settings.
- Modify: `Shinobi/UI/ShinobiMenuGameMode.h/.cpp`. The menu PC finds the stage (`TActorIterator<AShinobiMenuStage>`), calls `SetViewTarget` to it, and calls `Show(saved character, saved outfit)`.
- Test: `Tests/ShinobiMenuTests.cpp`

**Interfaces:**
- Consumes: Task 1 kit, Task 2 data, Task 3 `AShinobiMenuStage`.
- Produces:
  ```cpp
  enum class EShinobiMenuTab : uint8 { Play, Shinobi, Jutsu, Settings, Num };
  // UShinobiMainMenuWidget (public):
  void SelectTab(EShinobiMenuTab Tab);   // switcher index, 0.2 s cross-fade (render opacity in NativeTick), stage SetFocus, focus first widget of the tab
  EShinobiMenuTab GetTab() const;
  UWidget* GetTabPage(EShinobiMenuTab Tab) const; // the page widget, for later tasks to fill and for tests
  AShinobiMenuStage* GetStage() const;   // may be null (tests, a level without one)
  ```
- Layout:
  - **Top bar:** tab labels (heading 16, accent underline on the active tab) and, on the right, the Steam name (`GetOwningPlayer()->PlayerState->GetPlayerName()`) and the LAN/Steam line.
  - **Keys:** Q/E and Gamepad_LeftShoulder/RightShoulder cycle tabs in `NativeOnKeyDown` (also bubbled from children). Esc/Gamepad_FaceButton_Right selects PLAY.
  - **PLAY page:** sub-toggle HOST | FIND GAMES.
    - **Host:** a left list of map rows in `GetHostOrder()` order. Each row is a 96×54 `LoadArt(MapImagePath)` thumbnail (accent gradient when missing), the name, and a chip (CO-OP / PVP / TEST). Selecting or focusing a row sets the full-screen background `UImage` to that map's art at 35% darkened, falling back to transparent so the stage shows through. The right panel shows the name (heading 40), the blurb, "Up to N players", and the mode toggle from `GetModes`. **HOST** is a primary button calling `HostGame(MapIndex, Mode)`. QUIT is a small button bottom-left.
    - **Find Games:** a REFRESH button calls `FindGames()`. Rows show the thumbnail via `FindMapByName`, host, map, format, players x/y and ping. There is a "No games found — host one, or refresh" empty state, and a "Searching…" state while busy.
  - **SETTINGS page:** an embedded `UShinobiSettingsWidget` with `bMainMenu = true`.
  - **SHINOBI and JUTSU pages:** empty `UOverlay` placeholders, filled by Tasks 5 and 6.
  - The message line (`GetMessage()`) sits bottom-centre on every tab.

- [ ] **Step 1: Write failing tests:**
  - `MainMenuTabFocus`: create the widget on a test PC/world (or `CreateWidget` with the test world's game instance), add it to the viewport if a viewport exists, otherwise just `TakeWidget()`. For each tab, `SelectTab(Tab)` makes `GetTab() == Tab`, and the focused widget's user widget is inside `GetTabPage(Tab)`. If no slate application is available, assert the widget the tab chose as its focus target (expose `UWidget* GetFocusTarget(EShinobiMenuTab) const`) is non-null and inside the page.
  - `MainMenuHostRowsCoverMaps`: the PLAY page builds exactly `GetMaps().Num()` host rows.
- [ ] **Step 2: Build and confirm they fail.**
- [ ] **Step 3: Implement** the shell, the PLAY tab, the SETTINGS embed and the stage hook in `AShinobiMenuPlayerController::BeginPlay`. Keep the `-ShinobiAutoHost*` / `-ShinobiAutoJoin` code untouched.
- [ ] **Step 4: Run** `run_tests.ps1 -Filter ShinobiCoop.UI`. Expected: all pass. Then launch the menu (`Scripts/play.ps1` or `UnrealEditor.exe ShinobiCoop.uproject /Game/Shinobi/Maps/Lvl_MainMenu -game -windowed -ResX=1280 -ResY=720`). Click and pad through PLAY and SETTINGS, and host London to check the auto-travel still works.
- [ ] **Step 5: Commit** with `"feat: tabbed main menu: map cards with mode toggle, server browser, settings tab"`.

### Task 5: SHINOBI tab

**Files:**
- Create: `Shinobi/UI/ShinobiRosterPage.h/.cpp` (`UShinobiRosterPage : UUserWidget`, placed in the SHINOBI page)
- Modify: `Shinobi/UI/ShinobiMenuWidgets.cpp` (add the page)
- Test: `Tests/ShinobiMenuTests.cpp`

**Interfaces:**
- Consumes: `ShinobiSettings::Get/SetSaved*`, `ShinobiMenuArt::PortraitPath/LoadArt`, `AShinobiMenuStage::Show`, `AShinobiPlayerCharacter::IsOutfitBuilt`.
- Produces: `UShinobiRosterPage` with `TWeakObjectPtr<AShinobiMenuStage> Stage` (set by the shell), `void Pick(int32 RosterIndex)` and `void StepOutfit(int32 Direction)`. Both save and call `Stage->Show`.
- Layout:
  - **Bottom:** a centred strip with one 120×160 sheared card per `Roster[]` entry. Each card shows the portrait texture, or the accent gradient plus the name when missing, with the name label below. The selected card is raised 8 px and scaled 1.08 with an accent border. Cards are focusable buttons, and left/right moves between them.
  - **Right panel:** "SHINOBI" (dim), the name (heading 44), and an outfit row "◀ <Outfit> ▶". The row appends " (not built)" when `!IsOutfitBuilt(outfit, shown body)`. Gamepad: LT/RT or the d-pad up/down on the row steps the outfit.
  - The stage uses focus `Left`.

- [ ] **Step 1: Write a failing test.** `MenuRosterPickSaves`: create the page with no stage. `Pick(1)` makes `GetSavedCharacter() == Roster[1].Name`. `StepOutfit(+1)` from None gives `Outfits[1].Name`, and `StepOutfit(-1)` from None wraps to the last outfit. Restore and flush the config.
- [ ] **Step 2: Build and confirm it fails.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `-Filter ShinobiCoop.UI.Menu`. Then launch the menu and pick each character and outfit. The stage updates with no hitch longer than one frame after the first load.
- [ ] **Step 5: Commit** with `"feat: SHINOBI tab: roster portrait cards and outfit picker on the live stage"`.

### Task 6: JUTSU tab (Skills screen menu mode + new layout)

**Files:**
- Modify: `Shinobi/UI/ShinobiSkillsWidget.h/.cpp`
- Modify: `Shinobi/UI/ShinobiMenuWidgets.cpp` (embed in the JUTSU page)
- Test: `Tests/ShinobiMenuTests.cpp`, plus the existing `Tests/ShinobiLoadoutTests.cpp` run as a guard

**Interfaces:**
- Consumes: `ShinobiLoadout::GroupOf/GroupName/LoadSaved/Save/Place/Swap/Clear/IsValid`, `ShinobiKeyBindings::KeyLabel`, the style kit.
- Produces on `UShinobiSkillsWidget`:
  - `bool bMenuMode` (public; set before construct).
  - `void SelectJutsu(int32 PoolIndex)` (detail panel).
  - `const ShinobiLoadout::FSlots& GetShownLoadout() const`.
- **Menu mode:**
  - `Known` comes from `GetDefault<AShinobiPlayerCharacter>()->GetKnownJutsu()` and the loadout from `ShinobiLoadout::LoadSaved(Known)`.
  - `Request(Slots)` checks `IsValid`, then calls `Save`, updates the shown loadout and flushes `GGameUserSettingsIni`.
  - No combat, cooldown or lock overlays. `NativeTick` must not `Close()` without a pawn. Esc/K never closes; the shell owns Esc.
- **Layout (both modes):** the K screen gets it too.
  - **Left:** the pool, grouped under `GroupName` headings, 6 columns, orange dot on equipped ones.
  - **Right:** a detail panel for the hovered or focused jutsu: group (dim), name (heading 30), `Description`, a stat grid with Chakra `ChakraCost`, Cooldown `Cooldown` s and Seal time `WindUp` s (one decimal).
  - **Bottom:** the 8-slot bar with `KeyLabel` under each slot.
  - Drop the tooltip, because the panel replaces it.
  - In-game it keeps its lock banner and behaviour.

- [ ] **Step 1: Write a failing test.** `MenuLoadoutSavesValid`: save the current loadout config. Write an invalid saved array (`GConfig->SetArray` with 3 entries). Create the widget with `bMenuMode = true` and build it with `TakeWidget()`. `GetShownLoadout()` equals `Default()`. Simulate an edit with `OnCellClicked` (a pool cell for a jutsu not in the bar, then slot 7). `LoadSaved(Known)` now contains that jutsu in slot 7, and `IsValid` holds. Restore and flush.
- [ ] **Step 2: Build and confirm it fails.**
- [ ] **Step 3: Implement** the menu mode and the layout.
- [ ] **Step 4: Run** `-Filter ShinobiCoop.UI` and `-Filter ShinobiCoop.Loadout`. Expected: all pass. Then:
  - Launch the menu, equip a jutsu and quit.
  - Start Training Grounds and check the bar shows that jutsu (`LoadSaved` on spawn).
  - Press K in-game and check the new layout, with the lock banner still working in combat.
- [ ] **Step 5: Commit** with `"feat: JUTSU tab: loadout editing from the main menu with jutsu details; K screen shares the layout"`.

### Task 7: Portrait and map image capture + import

**Files:**
- Create: `Shinobi/UI/ShinobiMenuShots.cpp/.h` (`UShinobiMenuShotSubsystem : UWorldSubsystem`, so no edit to the shared controller)
- Create: `Scripts/menu_shots.ps1`, `Scripts/setup_menu_art.py`
- Output: `Content/Shinobi/UI/Menu/T_Portrait_<Name>.uasset`, `T_Map_<Id>.uasset`

**Interfaces:**
- Consumes: `AShinobiMenuStage` (`Show`, `GetCamera`), `ShinobiMenuArt::MapId`, `PortraitPath`.
- Produces:
  - **Flag `-ShinobiMenuPortraits`** (in Lvl_MainMenu): for each `Roster[i]`, it calls `Show(i, 0)` and sets the stage to a portrait framing: head to knees, 3:4, the camera's offset from the body only. It waits 1.5 s, then runs `HighResShot 768x1024 filename=ShinobiMenuPortrait_<Name>` with the UI hidden. Quits at the end.
  - **Flag `-ShinobiMapShot`** (any level): after 6 s (streaming), it hides the HUD and every pawn and takes `HighResShot 1920x1080 filename=ShinobiMapShot_<MapId>` from the view. The view comes from a per-level override actor tagged `ShinobiMenuShot` (a `CameraActor`) when the level has one. Otherwise it uses the first PlayerStart, raised 400 cm and pulled back 900 cm, looking along its forward with 15° down pitch. Quits.
  - `menu_shots.ps1`: runs the portrait pass once and the map pass once per `GetMaps()` path (list mirrored in the script; comment that it must match `GetMaps()`), then copies PNGs to `Saved/MenuArt/`.
  - `setup_menu_art.py`: imports `Saved/MenuArt/*.png` as `T_Portrait_<Name>` / `T_Map_<Id>` into `/Game/Shinobi/UI/Menu`. Compression `TC_Default`, sRGB on, no mips (`TextureGroup UI`). Re-run safe (reimport).

- [ ] **Step 1: Implement** the subsystem and both scripts. No automated test: this is capture tooling, verified by its output.
- [ ] **Step 2: Run** `powershell -File Scripts/menu_shots.ps1`. Expected: 1 portrait per roster entry and 9 map shots listed. Open 3 of them (Read tool) and check the framing. If a map's shot is unusable (no PlayerStart, or a wall in view), add its camera transform to a `SHOT_CAMERAS = {"<MapId>": (loc, rot)}` dict in `setup_menu_art.py`. A `--cameras` mode places a `CameraActor` tagged `ShinobiMenuShot` in that level and saves it. Run that mode, then re-shoot only that map. Add only the cameras that are needed.
- [ ] **Step 3: Run** `run_python.ps1 -Script Scripts/setup_menu_art.py` (editor closed). Expected: every texture exists. `run_tests.ps1 -Filter ShinobiCoop.UI.MenuArt` passes, and then add `MenuArtAllCaptured`: `LoadArt` is non-null for every roster entry and map.
- [ ] **Step 4: Launch the menu** and look at the SHINOBI and PLAY tabs with the real art.
- [ ] **Step 5: Commit** with `"feat: captured roster portraits and map images for the menu"`, including the `.uasset` files, scripts and source.

### Task 8: Menu tour screenshots, full suite, wrap-up

**Files:**
- Modify: `Shinobi/UI/ShinobiMenuShots.cpp`. Add `-ShinobiMenuTour`: in Lvl_MainMenu, select each tab, plus HOST with an arena selected and the 2v2 mode, plus FIND GAMES empty. Take a 1280×720 screenshot of each `ShinobiMenuTour_<n>.png` and quit.
- Modify: `Scripts/menu_shots.ps1` (`-Tour` switch).

- [ ] **Step 1: Implement and run the tour.** Read every screenshot. Fix overlaps, clipped text, unreadable contrast and the stage character hidden behind a panel. Re-run until clean.
- [ ] **Step 2: Run the full suite** with `powershell -File Scripts/run_tests.ps1`. Expected: `ALL N TESTS PASSED`. Also run `Scripts/smoke_m4.ps1`, which covers host/join and loadout replication through the menu.
- [ ] **Step 3: Commit** with `"feat: menu tour screenshots"`. Then report to the user with the tour screenshots and ask about merging `feature/main-menu` into `main`. When merging, the shared tree's checkout stays on `feature/outfit-fitting`. Merge with plumbing (`git merge-tree --write-tree main feature/main-menu` + `commit-tree` + `update-ref refs/heads/main`), per the git-merge-editor-lock memory.
- [ ] **Step 4: Save a project memory** `main-menu.md` covering the tabs, the stage actor, the capture flags and scripts, the commit-helper approach and any gotchas found. Add one line to `MEMORY.md`.
