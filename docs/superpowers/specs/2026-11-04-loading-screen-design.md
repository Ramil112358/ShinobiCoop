# Loading screen

**Problem.** Hosting, joining, a rematch, leaving or a disconnect all go through `OpenLevel` / `ClientTravel` / `ServerTravel`. The engine's `LoadMap` blocks the game thread, so the last frame of the old map (usually the menu) stayed frozen on screen until the new map was in. After that the world appeared at once and was still unfinished: World Partition cells and textures streaming in, shaders and PSOs compiling (hitches, missing objects), and made characters still being built by Mutable. Joining showed nothing at all while connecting. To the player it looked stuck.

## What other games do

Recent games agree on a few rules. They are the brief for this screen.

| Pattern | Seen in | Why |
|---|---|---|
| The destination's art, name and mode on the screen (not a generic splash) | Valorant, Fortnite, Apex, Overwatch 2, Spider-Man | Tells the player where they're going. The wait reads as part of the trip. |
| Something that is always moving: a spinner, an animated logo, a slow push-in on the art | Console certification (a load mustn't look like a hang); Destiny 2, Elden Ring, God of War | A frozen frame reads as a crash. Movement says it's working, and a hitch shows as a hitch, not a hang. |
| The screen is drawn on its own thread through the blocking part | UE's MoviePlayer (Fortnite, Lyra, most UE titles), other engines' loading threads | The game thread can't draw while `LoadMap` runs. |
| It stays up until the world is ready to play, not merely loaded: streaming done, the player spawned, shaders warm | Lyra's `CommonLoadingScreen` (`ILoadingProcessInterface`, "hold for additional seconds" for texture streaming), Fortnite, Call of Duty | Taking the screen down when `LoadMap` returns shows the pop-in and hitches the screen was meant to hide. |
| Shader compilation counts as loading, with a count shown | The Last of Us Part I PC, Call of Duty, Horizon Zero Dawn / Forbidden West PC, Hogwarts Legacy | Compiling while you play is the main cause of stutter on PC (DX12 PSOs). |
| Honest progress: named steps, never a fake timed bar | Call of Duty (its shader-compile percentage), The Last of Us Part I PC | A bar that fills and then sits at 99% does more harm than a spinner. |
| Rotating tips that use the player's own bindings | Elden Ring, Apex Legends, Fortnite | Teaches something during the wait. A key picture that's wrong after rebinding teaches the wrong thing. |
| A minimum time, a short settle, then a fade | Lyra (`MinimumLoadingScreenDisplayTime`, hold seconds), most AAA | No one-frame flash on a fast load. The first frame you see is a settled one. |
| Input held while it's up | Lyra (`FLoadingScreenInputPreProcessor`) | Keys pressed during the load mustn't fire jutsu or open menus behind the screen. |

## Design

`UShinobiLoadingScreenSubsystem` (game instance subsystem, `Shinobi/UI/ShinobiLoadingScreen.*`) owns one screen. A pure `ShinobiLoadingScreen::Evaluate` decides what it's waiting for, so tests can drive it without a world.

- **Look** (Slate, laid out for 1080p, in the menu style: dark, dusk-orange accent, bold italic caps):
  - The map's menu picture (`T_Map_<Id>`, the same art as the HOST cards) fills the screen. It pushes in from 103% to 110% over 25 s with a slight drift (a Ken Burns move). Gradients darken the bottom, the left and the top so the text stays readable.
  - The main menu (and start-up) use Hidden Leaf Valley's picture, darker, titled SHINOBI COOP.
  - **Bottom left:** the mode in the accent colour (`CO-OP · UP TO 4 PLAYERS`, `PVP ARENA · 2V2`, `TEST LEVEL`, `MAIN MENU`), the map name in large caps, an accent rule and the map's blurb from `GetMaps()`.
  - **Under that, a TIP:** one every 8 s with a half-second crossfade, starting at a random one. Tips use the player's current bindings, keyboard / controller ("Dodge (Left Ctrl / B) …"). Co-op and arena have their own sets (revive and villagers, or rounds and no revives).
  - **Bottom right:** the step being waited on (CONNECTING, LOADING MAP, STARTING GAME, STREAMING WORLD, COMPILING SHADERS, PREPARING CHARACTERS, STREAMING TEXTURES), a detail line ("212 left", "3 areas left", "Placing your shinobi"), a turning, breathing accent arc, and a thin progress bar.
    - The bar eases toward the step's progress and never moves back. A light sweeps along it. Within COMPILING SHADERS it fills by the share of shaders done.
- **When it goes up:**
  - The session subsystem calls `Begin` before it travels: host (map and format known), join (map and format from the session's settings, so the screen shows the destination while connecting), leave and return to menu.
  - Any other load (a rematch's `ServerTravel` on host and clients, a disconnect's trip to the default map, the first map at start-up) brings it up from `PreLoadMapWithContext`, for this game instance only. The same map as last time keeps its format, so a client's rematch still reads 2V2.
  - PIE starts with it up, since PIE loads no map.
- **Through the blocking load:**
  - In a game build (`-game` or packaged), the subsystem hands the engine's MoviePlayer a copy of the screen. The MoviePlayer draws it on the Slate loading thread, so the spinner, the art move and the tips keep going while `LoadMap` blocks. Every animation runs on the clock of the view's build time, so the MoviePlayer's copy and the viewport's copy match when one hands over to the other.
  - Without the MoviePlayer, Slate is ticked once, so the frozen frame is the screen and not the old map. PIE has no MoviePlayer, so the screen is static during the load itself.
- **After the load:** the screen sits on the game viewport (Z 20000, above every menu) while the world renders behind it. That rendering is what compiles the shaders and streams the textures the screen waits for. It comes down only when all of these hold, in order:
  1. No pending net game, travel URL, server travel or seamless travel.
  2. The world has begun play and has a game state.
  3. The local player has a pawn. Not on the menu map; given up after 10 s, for spectating or a slow spawn.
  4. No streaming level (including World Partition cells) is loading or changing visibility.
  5. No shaders left: pipeline cache precompiles, active PSO precache requests and, in the editor, shader compile jobs.
  6. No Mutable body still building (`UShinobiBodyComponent::IsMutableBuildPending`).
  7. Texture streaming has nothing left wanting mips. This one is held for at most 4 s, as Lyra's "additional seconds" are.
- **Then:** it stays up at least 1 s in all, plus a 0.35 s settle once everything is ready, then fades out over 0.45 s. Input goes back to the game as the fade starts. If anything starts loading again during the fade, the screen comes back up.
- **Safety:** 45 s after the map loaded (or 135 s after the screen went up), it comes down whatever is still pending, and logs what it was waiting for. `Loading screen: up for …`, `ready after N s` and `gave up waiting on …` are in the log.
- **Input:** outside the editor, an input preprocessor holds key presses, mouse buttons, the wheel and the sticks. Releases pass, so nothing stays held after the screen; Alt+F4 passes too. The screen also swallows clicks. In the editor the preprocessor is skipped, because it would hold the editor's own keys (Shift+F1, Esc).
- **Off:** `Shinobi.LoadingScreen 0`, `-NoLoadingScreen`, `-unattended` (every shot and smoke script passes it, so captures are never covered), `-nullrhi`, automation tests, commandlets and dedicated servers (the subsystem isn't created).

## Testing

`ShinobiCoop.UI.LoadingScreen.*`:

- **Steps:** the steps come in order, progress always rises and ends at 1, and the shader count is shown. Textures hold the screen only until their time is up.
- **MapPath:** every hosted map is found by package path, object path and PIE name, and the menu is no map.
- **Tips:** every key token is filled in, lengths fit the panel, and co-op versus arena content.
- **OffInTests:** the screen is off under automation.

By hand:

1. Host each map from the menu (PIE and `Scripts/play.ps1`).
2. Join from a second instance. CONNECTING should show the destination.
3. Play an arena rematch.
4. Leave a game.
5. Kill the host. The client should fall back to the menu behind the screen.
