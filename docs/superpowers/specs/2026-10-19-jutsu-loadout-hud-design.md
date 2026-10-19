# Jutsu loadout, 8-slot bar, Skills screen and Shortcuts — design

Date: 2026-10-19 · Status: approved in brainstorming, pending plan

## Why

Today the 19 player jutsus live on 5 fixed pages of 4 (`UShinobiJutsuAbility::Page` + `InputID` Jutsu1-4). Keyboard players press 1-4 and cycle pages with Q, so reaching a jutsu on another page costs extra presses mid-fight and you can only see 4 at a time. The user wants an MMORPG-style setup (reference: Throne and Liberty): a compact bar of chosen jutsus always visible, a Skills screen to pick which jutsus are equipped, and rebindable keys.

Success: a player sees all 8 equipped jutsus at once, fires any of them with one key press (keyboard) or RB + face button (gamepad), chooses their 8 out of the 19 on a Skills screen, and can rebind every slot and core action.

## Goals

1. **8-slot jutsu bar** on the HUD replaces the 4-slot page bar.
2. **Skills screen (K)** to equip/reorder jutsus into the 8 slots.
3. **Shortcuts screen** (Settings → Shortcuts) to rebind keyboard/mouse keys for the 8 slots and core actions.
4. Gamepad keeps a page system: 2 pages of 4 slots.

## Non-goals

- Skill levels, upgrades, skill points, unlocks (T&L's numbered badges). All 19 jutsus are always known.
- Gamepad button rebinding.
- Rebinding WASD movement, camera look/zoom/free-look, or Esc.
- Per-character loadouts: the roster is cosmetic only (every character has the same `StartupAbilities`), so one loadout per PC.
- Named/multiple saved loadout presets.

## Anti-goals (would be a failure even if it works)

- A swap that lets a player dodge a cooldown (unslot a jutsu on cooldown, re-slot it ready).
- Swapping mid-fight.
- A client able to fire a jutsu that is not in its slots, or to change its loadout without the host agreeing.
- A bindable Alt+F4 / Alt+Tab, or any rebind that leaves a slot/action with no key by accident.
- Breaking existing second-press behaviour (Kamui exit, Teleport jump back, dragon dismount) or mounted-dragon controls.

## User decisions

### HUD bar
- **One row of 8 slots**, bottom centre, smaller icons than today's 4-slot bar. A small gap after slot 4 marks gamepad page 1 | page 2. (Layout "A, two rows" was chosen when there were 19 slots; with 8 the user approved a single row.)
- Each slot shows: icon, key label below, chakra cost in the corner, a dark overlay with a cooldown countdown, and a desaturated "not enough chakra" look. **No name text under icons** (name can show on hover or while casting).
- Empty slots show an empty frame with the key label.
- Key labels switch automatically between keyboard and gamepad glyphs based on last input device.
- Gamepad: the 4 slots on the current page get a bright frame and face-button labels; the other 4 dim. The whole bar stays visible.

### Keys (defaults, all rebindable)
| Slot / action | Default key |
|---|---|
| Slots 1-4 | 1, 2, 3, 4 |
| Slots 5-8 | R, T, G, Caps Lock |
| Skills screen | K |
| Melee | Left Mouse |
| Dodge | Left Ctrl |
| Sprint | Left Shift |
| Block | F |
| Chakra charge | C |
| Revive | E |
| Lock-on | Tab |
| Jump | Space |
| Dismount dragon | X |
| Map | M |

- **Alt + key** combos are allowed as bindings (Alt is the modifier; Ctrl/Shift stay dodge/sprint and are not modifiers).
- Q no longer cycles pages on keyboard; it is free to bind. Mouse 4 / Mouse 5 / Middle Mouse are free to bind.
- **Gamepad (fixed):** RB + face buttons fire the current page's 4 slots (A/B/X/Y = slot 1-4 or 5-8 in today's order Bottom/Right/Left/Top), D-pad left/right (while RB held, as today) flips between page 1 (slots 1-4) and page 2 (slots 5-8).

### Default loadout
Slot 1 Fireball, 2 Chidori, 3 Rasengan, 4 Amaterasu, 5 Teleport, 6 Kamui, 7 Almighty Push, 8 Susanoo.

### Skills screen (K)
- Full-screen dim overlay titled "JUTSU". Top: grid of all 19 jutsus (7 per row). Jutsus currently slotted get a purple frame. Bottom: the 8 slots in bar order with key labels and the 4|4 gap.
- Equip: click a jutsu (gold outline) then click a slot, **or** drag the jutsu onto a slot. Equipping a jutsu that is already in another slot moves it (no duplicates). Dropping onto an occupied slot replaces it (the old one returns to the pool).
- Reorder: drag slot to slot (swaps the two).
- A slot may be left empty (drag a slot's jutsu back to the pool, or right-click the slot).
- Hover tooltip: name, chakra cost, cooldown, one-line description.
- Gamepad: D-pad/stick navigates, A picks a jutsu, A again picks a slot, B backs out/closes.
- **Opening on gamepad (my call):** every pad button is taken, so the pause menu gets a "Jutsu" entry (Resume / Jutsu / Invite Friend / Settings / Leave) that opens the Skills screen. Keyboard players can use it too.
- K or Esc closes. The game does not pause (co-op).
- Mouse cursor shows while the screen is open; gameplay input is blocked.

### Swap rules
- **Only out of combat.** In combat = within **5 s** after the player last dealt damage to an enemy or took damage. The screen still opens while in combat (to look), with a banner "In combat: swaps unlock 5s after the last hit" and swaps disabled.
- **A jutsu on cooldown is locked** in its slot: shows a lock and its timer, cannot be moved, reordered or replaced until the cooldown ends.
- Other blocked moments (my call, follows the above):
  - **Whole loadout locked:** while casting a jutsu, while mounted on a dragon, while downed.
  - **Only that jutsu's slot locked:** Kamui while phased, Teleport while its mark is set, Susanoo while active, a summon while it is out. Other slots can still be swapped.
- **What counts for the 5 s timer (my call):** any damage the player deals to an enemy or takes from an enemy, recorded where damage is applied, so damage over time (Amaterasu burns, fire patches) and hits from the player's own summons and dragon breath count. Self-damage and teammate damage don't count. A long burn keeping you in combat is intended.

### PvP arena (`AShinobiPvpGameMode`, 1v1 / 2v2 rounds)
- The saved loadout carries into PvP.
- Opposing players count as enemies for the 5 s in-combat timer.
- Swaps are allowed during the pre-round countdown (`State.PvpHold`) and between rounds.
- Swaps are locked for the whole of a live round, even with no hits for 5 s.

### Shortcuts screen
- Settings → Shortcuts, reachable from the main menu and pause menu Settings (`UShinobiSettingsWidget`).
- Two columns: **Jutsu slots** (Slot 1-8, each showing the jutsu currently in it in grey for reference, plus "Skills screen") and **Actions** (Melee, Dodge, Sprint, Block, Chakra charge, Revive, Lock-on, Jump, Dismount dragon, Map).
- Click a key → "Press a key…" → press the new key (with or without Alt). Esc cancels.
- **Pressing a key already in use swaps** the two bindings. Nothing is ever left unbound by a rebind.
- Reset to defaults button. Back returns to Settings.
- Keyboard & mouse only.
- Blocked keys: Esc, Windows keys, Alt+F4, Alt+Tab. Gamepad keys are ignored while listening. (My call: also block WASD and Right Mouse, since they stay fixed for movement and free-look, so binding them would conflict.)

### Saving
- Loadout and key bindings save per PC (like graphics settings, `GameUserSettings.ini` via `GConfig`, as `ShinobiSettings` does). A client's settings never affect other players' keys.

## Behaviour rules

- **Second presses follow the slot.** Kamui exit fires from Kamui's slot, Teleport jump from Teleport's slot, dragon dismount from the summoning dragon's slot. Today these are separate ability classes sharing the parent's `InputID` and requiring `JutsuPage(n)`; they must instead follow whatever slot their parent jutsu currently occupies.
- **Mounted on a dragon:** today page 5 slot 1 dismounts and X dismounts on any page; melee becomes dragon breath. Keep: X (Dismount action) works always. The slot of whichever dragon you summoned (Tairn or Crimson) dismounts; the other dragon's slot does nothing while mounted.
- **Networking:** the client sends its loadout to the server on spawn and on each swap; the server validates (known jutsu, no duplicates, 8 slots max, swap rules: not in combat, not on cooldown, not casting) and is the authority for which jutsu each slot fires. A rejected swap is reverted on the client (like `ClientSetJutsuPage` today). Joining clients and respawns re-send the saved loadout.
- The gamepad page is local-only presentation + routing (which 4 slots RB+face reaches); it no longer gates abilities on the server.

## Current code this touches (facts)

- `Shinobi/ShinobiInputConfig.{h,cpp}` builds all `UInputAction`s and two `UInputMappingContext`s in code (`IMC` + `IMC_ShinobiJutsu` added while RB held). `JutsuPageNext` on Q and D-pad right; `JutsuPagePrev` on D-pad left.
- `EShinobiAbilityInputID` (`Abilities/ShinobiGameplayAbility.h`): Melee, Dodge, Sprint, ChakraCharge, Jutsu1-4, Revive, Dismount, Block. ASC input ids are the enum values (`ShinobiCharacterBase.cpp:286` gives each spec its `InputID`).
- `UShinobiJutsuAbility::CanActivateAbility` gates on `ShinobiTags::JutsuPage(Page)`; `Page` defaults 1, set per class (2-5). Follow-ups (`UGA_KamuiExit`, `UGA_TeleportJump`, `UGA_DragonDismount`) add `JutsuPage(n)` to `ActivationRequiredTags`.
- Page state: `AShinobiPlayerCharacter::{Get,Set,Apply}JutsuPage`, `TryCycleJutsuPage`, `PageHasJutsu`, `ServerSetJutsuPage`/`ClientSetJutsuPage`, loose tags `State.JutsuPage.1-5`, `ShinobiTags::NumJutsuPages = 5`.
- HUD: `UI/ShinobiHUDWidget.{h,cpp}` built in C++; `NumJutsuSlots = 4`, `UpdateJutsuSlots`, page pips.
- Menus: `UI/ShinobiMenuWidgets.{h,cpp}` (`UShinobiMenuWidget` base: centred column of buttons, built in code); `UShinobiSettingsWidget` cycles rows; settings in `UI/ShinobiSettings.{h,cpp}`.
- Icons: `Jutsu->Icon` soft textures `/Game/Shinobi/UI/T_Jutsu_*`; `DisplayName`, `ChakraCost`, `Cooldown` on `UShinobiJutsuAbility`.
- No "in combat" state exists yet.
- Debug/test hooks drive pages: `AShinobiPlayerController::ShinobiPage` and the jutsu showcase timelines (`ShinobiPlayerController.cpp` ~lines 340-990), plus automation tests in `Source/ShinobiCoop/Tests/` (Input, Summon, Susanoo, Mount, MovementJutsu, Elemental, Damage, Block) that set pages.

## Left to the builder

- How slots map to ASC input: e.g. 8 slot input IDs replacing Jutsu1-4 with the server reassigning `Spec.InputID` per loadout (and follow-ups inheriting their parent's), or a slot→ability lookup on press. Must stay server-authoritative and predicted as today.
- Rebinding mechanism: rebuild the code-built mapping contexts from saved keys, or adopt Enhanced Input player-mappable keys — whichever fits the code-built actions with least churn. Alt combos need a chord/modifier approach that doesn't also fire the plain key.
- Description text for each jutsu's tooltip (one line each).
- Exact pixel sizes, art polish, drag visuals; match the existing gold/dark HUD style.
- Whether `Page` stays on `UShinobiJutsuAbility` (likely removed) and how debug commands (`ShinobiPage`) and showcase timelines migrate (e.g. a `ShinobiSlot`/`ShinobiLoadout` cheat).
- Updating the existing tests that rely on pages, plus new tests for loadout validation, swap rules, rebind swap/reset, and save/load.

## Mockups

Brainstorm mockups (gitignored): `.superpowers/brainstorm/1930-1792405872/content/` — `bar-layout-v2.html` (option A), `skills-screen.html`, `shortcuts-v2.html`.
