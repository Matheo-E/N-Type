# Game assets

All asset paths use lowercase `snake_case` names and are grouped by purpose:

```text
assets/
├── audio/
│   ├── music/
│   └── sfx/
├── backgrounds/
│   ├── level_01_ice/{day,night}/
│   ├── level_02_candy/{day,night}/
│   └── level_03_dentist_lab/{day,night}/
├── sprites/
│   ├── effects/
│   ├── enemies/{level_01_ice,level_02_candy,level_03_dentist_lab}/
│   └── player/
└── ui/
```

Each background ambience contains these four ordered layers:

1. `layer_01_far.png`
2. `layer_02_middle.png`
3. `layer_03_near.png`
4. `layer_04_ground.png`

It also contains a composite `preview` image. The level 3 day preview is a real
JPEG and intentionally remains `preview.jpg`; the other previews are PNG files.
The runtime mapping is centralized in `client/src/BackgroundConfig.hpp`.

`audio/sfx/button_click.mp3` was renamed from
`COMCam_Instamatic declenchement 1 (ID 2821)_LaSonotheque.fr.mp3`; its embedded
metadata is preserved.
