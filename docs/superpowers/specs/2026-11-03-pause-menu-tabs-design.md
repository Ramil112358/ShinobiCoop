# Pause menu tabs

**Problem.** The pause menu was a centred column (Resume / Jutsu / Invite Friend / Settings / Leave). Settings opened a second panel on top and collapsed the pause menu, Shortcuts opened a third, and Jutsu closed the menu to open the Skills screen. With nothing dimming the game behind them, the hidden menu's labels showed through, and the screen flickered between panels as you went in and out. Settings rows were plain buttons reading "Name: Value" that only cycled forward.

**Design.** The pause menu now works like the main menu (and most console or PC games' in-game menus): one screen with a tab bar, so nothing opens on top of anything else.

- **Tabs:** GAME · JUTSU · SETTINGS · CONTROLS. Q / E or LB / RB switch tabs.
  - **GAME:** the map and mode, then RESUME, INVITE FRIEND and LEAVE GAME. Leave asks first: the first press turns it into "CONFIRM LEAVE?" for 4 s. A PLAYERS card lists who's in the game, with ping, and marks you.
  - **JUTSU:** the Skills screen (`UShinobiSkillsWidget`, `bEmbedded`). It uses the live pawn's bar, with no backdrop or title of its own.
  - **SETTINGS:** the settings panel (`UShinobiSettingsWidget`).
  - **CONTROLS:** the key bindings (`UShinobiShortcutsWidget`), which used to be Settings > Shortcuts.
- **Keys:** Esc or Start resumes from any tab. B goes back to GAME first, then resumes. K opens the menu on JUTSU and closes it from there. A key being bound on CONTROLS, or B dropping a JUTSU pick, is used by the page and never reaches the menu.
- **Screen:** a full-screen dim, a top bar with the tabs and a "LIVE · the game keeps running" reminder, and a footer with session messages and key hints. The hints switch between keyboard and gamepad with the last device used. The HUD is hidden (render opacity 0, still ticking) while the menu is open.
- **Input:** UI only while the menu is open, as the Skills screen already was. Gameplay keys wait, so pressing 1 in the menu no longer casts. The world keeps running. Clicks on the backdrop stay in the menu, and focus that strays is taken back, so Esc always works.
- **Settings panel (main menu and pause menu):** rows sit under CHARACTER / GRAPHICS / DISPLAY / EFFECTS headings. Each row reads "Name ‹ Value ›". A click (or a click on ›) steps the value on, and a click on ‹ steps it back. Left / Right (arrows, D-pad, stick) step the focused row. A help line under the rows explains the hovered or focused row (`ShinobiSettings::RowDescription`). `ShinobiSettings::Step(Row, World, Delta)` steps both ways and wraps; `Cycle` is `Step(+1)`.
- **Main menu:** it gains a CONTROLS tab for the key bindings. Its SETTINGS tab loses the Shortcuts entry.
- **Shortcuts restyle:** the menu style (panel, sheared buttons, accent headings). The key being listened for is lit. Leaving the tab cancels the listen.
