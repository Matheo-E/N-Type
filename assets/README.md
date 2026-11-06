# Game assets

Project assets use lowercase `snake_case` names and are grouped by purpose.
The supplied FreeGUI package and game logo retain their original filenames:

```text
assets/
├── audio/
│   ├── music/
│   └── sfx/
├── FreeGUI/
│   ├── png/{Button,Window,BG}.png
│   └── ai/, cdr/, eps/, svg/  (source artwork, not loaded at runtime)
├── backgrounds/
│   ├── lobby/{back_play,back_menu}.png
│   ├── level_01_ice/{day,night}/
│   ├── level_02_candy/{day,night}/
│   └── level_03_dentist_lab/{day,night}/
├── sprites/
│   ├── effects/
│   ├── enemies/{level_01_ice,level_02_candy,level_03_dentist_lab}/
│   ├── La Contre-Attaque de M. Jean-cive.png
│   ├── lobby/bg_gradient.png
│   └── player/
```

`backgrounds/lobby/back_play.png` is the title illustration. Its red PLAY button
is taken from the empty red rectangle in `FreeGUI/png/Button.png` and is available
immediately. The menu uses `backgrounds/lobby/back_lobby.png` if supplied, falling
back to `back_menu.png`. `sprites/lobby/bg_gradient.png` is legacy artwork.

`sprites/La Contre-Attaque de M. Jean-cive.png` is the transparent game logo.
It appears at the top of the PLAY screen and above the main-menu buttons,
scaled proportionally with texture smoothing.

`FreeGUI/png/Window.png` provides panels, field surfaces and red ribbons;
`Button.png` provides colored buttons, settings/back icons and the score trophy.
`client/src/GuiSkin.hpp` shares their source rectangles across the launcher and
pause menu. Panels are split into nine rectangles to preserve their borders
when resized. Labels use system fonts rendered by SFML at a higher resolution,
then reduced to their display size. `BG.png` is supplied but not used: the menu
keeps the requested illustrated background.

`sprites/player/tooth.png` is a horizontal strip of three 724×724 frames. All
players loop through 0, 1, 2, 1 at 8 animation steps per second, even when idle.
The client draws an 88-pixel sprite around the server's 40×40 player bounds;
animation never changes those bounds.

Each background ambience contains these four ordered layers:

1. `layer_01_far.png`
2. `layer_02_middle.png`
3. `layer_03_near.png`
4. `layer_04_ground.png`

The runtime mapping is centralized in `client/src/BackgroundConfig.hpp`.

`audio/sfx/button_click.mp3` was renamed from
`COMCam_Instamatic declenchement 1 (ID 2821)_LaSonotheque.fr.mp3`; its embedded
metadata is preserved.

Music: `audio/music/lobby.mp3` plays throughout the title and launcher; each level
uses its matching soundtrack. `audio/sfx/button_click.mp3` supplies UI clicks.
Music and click-sound preferences survive restarts and returns to the menu.
