# Equipment placement reviewer

This standalone app follows current game-table equipment selectors, offsets and draw
order. Its code is in normal Git; catalogs, source art, reports and browser exports
remain local. Python dependencies: Pillow, NumPy and PyYAML.

From the repository root, with the optional HUD authoring bank available:

```sh
python3 -B tools/paperdoll_review/build.py
python3 -B tools/paperdoll_review/verify.py
python3 -m http.server 8899 --bind 127.0.0.1
```

Open <http://127.0.0.1:8899/tools/paperdoll_review/index.html>.
`build.py --inventory /path/to/bank` selects another authoring bank. The default
is the existing local `level_generation/ui/hud_inventory/` bank. The app consumes
its manifests/checkpoints and prepared artwork; it imports no code from that tree.
It reads current tables and the character layout directly from `assets_dev/engine/`.
Missing or changed frozen source/candidate images fail visibly during catalog build.

Generated `catalog.js`, native previews, optional report scripts and browser test
outputs are ignored. Building the catalog does not change source-bank checkpoints
or install artwork into the game. Without the optional authoring bank, the game
build still works; this authoring catalog cannot be regenerated.

Choose a character, equipment and pose, then inspect each original/candidate layer.
The 175x378 logical doll canvas is composited at 2x physical resolution. Body and
doll Y coordinates use the engine's sign conventions; z order comes from
`character.yml`. Armor, alternate hand poses, cloaks, off-hand rotation, bow and
fingers use the runtime selectors and registration tables. The app does not model
class, skill or inventory legality beyond those placement rules.

Flags, selected attempts and approvals remain in browser storage until **Export
review JSON** is used. Each decision binds source/candidate hashes. Existing browser
storage is preserved because the app remains served from the same origin/port.
