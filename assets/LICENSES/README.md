# Third-party assets in `assets/`

The fonts are compiled into the smg library (`resource_fonts.conf`), so every
program linking smg redistributes them. Their licenses travel with them here.

| File | Author | License |
|---|---|---|
| `Cousine-Regular.ttf` | Steve Matteson / Google (Croscore fonts) | SIL OFL 1.1 ([OFL-1.1.txt](OFL-1.1.txt)) |
| `DroidSans.ttf` | Ascender Corporation / Google | Apache 2.0 ([Apache-2.0.txt](Apache-2.0.txt)) |
| `JetBrainsMonoNerdFont-Regular.ttf` | The JetBrains Mono Project Authors; glyphs patched in by [Nerd Fonts](https://www.nerdfonts.com) | SIL OFL 1.1 ([OFL-1.1.txt](OFL-1.1.txt)) |
| `Karla-Regular.ttf` | Jonathan Pinhorn (Reserved Font Names "Karla", "Karla Tamil") | SIL OFL 1.1 ([OFL-1.1.txt](OFL-1.1.txt)) |
| `ProggyClean.ttf`, `ProggyTiny.ttf` | Tristan Grimmer | MIT, as distributed with [Dear ImGui](https://github.com/ocornut/imgui/tree/master/misc/fonts) |
| `Roboto-Medium.ttf` | Christian Robertson / Google | Apache 2.0 ([Apache-2.0.txt](Apache-2.0.txt)) |
| `SourceSansPro-Regular.ttf` | Adobe | SIL OFL 1.1 ([OFL-1.1.txt](OFL-1.1.txt)) |
| `binary_to_compressed_c.cpp` | Omar Cornut (Dear ImGui) | MIT |
| `cute_goose.jpg` | unknown | test image only (`test/test_maptotexture.cpp`), not built into the library |

The copyright line for each OFL font is embedded in the font file itself.

Everything else in `assets/` (`smg.jpg`, `smg.ico`, `smg_example.png`, the `.conf`
and `.rc` files) is part of smg and under its [MIT license](../../LICENSE).
