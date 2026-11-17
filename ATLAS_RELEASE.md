# OpenYAMM Atlas 0.3.0 for Windows

This Windows x64 preview runs the MM6, MM7 and MM8 Atlases on OpenYAMM 2.6. Atlas UI text has been checked in English and Polish. No Android package accompanies this release.

## Install

1. Extract `OpenYAMM-Atlas-0.3.0-windows-x64.zip` into a new folder.
2. Run `Play Atlas.cmd`. Allow PowerShell to run the bundled setup script for this process. It does not change the system execution policy.
3. The first run downloads the official 2.6 desktop packs and Polish language packs. The game payload is several GB; the ZIP does not contain it.
4. Choose your starting world in New Game. During play, click the minimap to open the Map Book and choose Atlas.

The launcher checks the included 2.6 index and reuses completed packs on subsequent runs. Downloads and imports are checked against their expected sizes and SHA-256 hashes. Existing local files of the expected size are reused without hashing the whole payload every time. Failed downloads leave completed packs available for the next attempt.

The content index is pinned to 2.6. The script never substitutes the latest release's content. Packs come from official OpenYAMM hosting, whose availability is controlled upstream. Keep the downloaded `assets` folder so it can be reused.

Saves, settings and the log belong to this extracted folder. Keep it separate from your official OpenYAMM installation. The game's normal official binary updater is disabled for this Atlas package; use Atlas releases for executable updates.

## Reuse existing content

To avoid downloading matching packs again, open PowerShell in the extracted Atlas folder and run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Setup-Content.ps1 -ExistingAssets "C:\Games\OpenYAMM\assets"
```

Pass an existing assets folder, its `packs` folder, or the installation root. Matching 2.6 files are verified, then hard-linked on the same volume or copied when links are unavailable. The source installation and its saves are not modified. Missing or nonmatching packs are downloaded from official hosting.

Then run `Play Atlas.cmd` normally. Keep the Atlas folder's downloaded content when moving to a later Atlas release that can reuse it.

## Atlas switch and language

The main menu has **Interactive Atlas: On / Off**, or **Atlas interaktywny: WŁ. / WYŁ.** in Polish. Changes apply immediately and survive a restart. Turning it off hides the Map Book entry and leaves discoveries and notes intact.

Choose English or Polish in Settings and restart after changing language. The package includes the Atlas Polish supplement and downloads the official Polish packs. It does not contain the privately generated Polish MM9 retail voices, movies or reviewed text overlay. The source includes an importer for owners of that edition.

## Saves

2.6 uses upstream save format 191. Migration from test copies of 2.5 format-184 saves passed. Back up campaigns before moving between builds; new saves are not promised readable by 2.5.

The official 2.6 game can read the tested Atlas saves. Its resave omits the supplemental Atlas trailer, so a return trip loses some Atlas knowledge. It does not preserve the whole extension. The much older Extended version-84 format is a separate compatibility issue and must not be treated as an ordinary upstream save.

## Troubleshooting

- If setup fails, run `Play Atlas.cmd` again. Completed content is retained.
- Always start with the bundled launcher so missing packs are supplied from the pinned index.
- The Atlas needs this compiled executable. Copying `mods` into stock OpenYAMM alone does not add its interface.
- For a game failure, include `openyamm.log`, `atlas-release.json`, the world/map and reproduction steps in an issue. Do not publish your personal campaign unless you want to share it.
- Native checks cover Windows/OpenGL. Android and a physical controller were not tested for this release.

## Rebuild the small package

Build an Atlas-enabled Windows Release as described in `ATLAS_PORT.md`. Stage the overlays with `tools/stage_atlas_overlay.py`. Put the built `OpenYAMMAtlas.exe` (or `openyamm.exe`), its three MinGW runtime DLLs and `runtime/shaders/glsl` in a runtime staging folder. With the verified official 2.6 content index:

```sh
python3 packaging/atlas/package_windows.py \
  --runtime /path/to/windows-runtime \
  --overlay /path/to/staged-overlay \
  --content-index /path/to/official-2.6-index.json \
  --source-root . --output output/release \
  --version 0.3.0 --code-commit <compiled-source-commit>
```

The packager whitelists the Atlas language supplement and does not copy campaigns, runtime packs or local MM9 retail media. The ZIP has a file manifest; `SHA256SUMS` records the archive hash. Runtime packs stay outside Git.
