# Font Upscale Scratch Tools

These scripts are portable authoring tools. Font masters, restoration banks and generated previews remain local;
restore those optional banks to use the linked studies below. Installed fonts ship separately under
`assets_dev/engine/fonts`. The tools and their tests need the optional FontTools, NumPy, Pillow, SciPy,
`pathops` and `potrace` Python modules; the game build does not need them.

See the [complete font inventory and replacement workflow](FONT_REPLACEMENT_PLAN.md) for the active-game backlog,
source exceptions, and the recommended production and validation process.

The completed [AI-assisted dialogue TTF reconstruction](dialogue_ai/README.md) includes a usable Arrus-derived
TrueType font, an interactive specimen, and reproducible tracing/validation tools.

The newer [faithful Arrus specimen](arrus_faithful/README.md) compares manually refined native letter shapes with
the bitmap and AI versions. Version 0.200 preserves its 38 earlier masters, adds smooth capitals/digits and shares
accented bases: 71 masters and 56 compositions. The remaining symbols retain native traces.

The [faithful Create restoration](create_faithful/README.md) is also active. Its configurable manifest, source
metrics, editable stroke masters and shared font pipeline preserve native spacing and reproduce the font offline.
The [2026-09-28 dialogue review](dialogue_refinement_20260928/index.html) fixes unintended horizontal offsets in
both Arrus and Create and checks long-dialogue fallback. Create 1.100 also refines the letter shapes highlighted
in image 2099, including “What”, “Every” and “choice”, while preserving native text widths.

The [faithful Lucida restoration](lucida_faithful/README.md) preserves the original oblique sans-serif design,
with full glyph proofs and original/new character, service, spellbook and HUD comparisons. Version 1.100 fixes
unintended glyph offsets and refines the lowercase; its interactive review includes the sentence from image 2090.
The user approved this replacement on 2026-09-28; it is installed in the game and active UI studies.

The [faithful SMALLNUM restoration](smallnum_faithful/README.md) restores the full compact alphabet and digits,
with inspection and counter comparisons at native and enlarged UI sizes.

The [faithful Comic restoration](comic_faithful/README.md) preserves the handwritten design and native spacing,
with item statistics and Arcomage comparisons at native and enlarged UI sizes.

The [faithful Book2 restoration](book2_faithful/README.md) preserves the calligraphic design with an elliptical nib,
with original/new journal and travel-screen comparisons at native and enlarged UI sizes.

The [faithful AUTONOTE restoration](autonote_faithful/README.md) preserves the thin upright journal face,
with explicit pixel-1 ink extraction and a single black layer without a shifted shadow.

The [faithful ENDGAME restoration](endgame_faithful/README.md) preserves the broad-pen certificate face and
old-style numerals, with native/new production victory-screen captures and an unshadowed tintable descriptor.

The [remaining six-font refinement review](remaining_refinement_20260928/README.md) audits SMALLNUM, Comic,
Book2, AUTONOTE, ENDGAME and SPELL against all native glyphs and the previous TTFs. It corrects glyph origins
throughout and refines selected calligraphic, journal, certificate and beacon letterforms. The user approved all six
on 2026-09-28; they are installed in development assets, the engine package and their active UI bundles.
The [editable comparison](remaining_refinement_20260928/index.html) preserves the before/after review, and the
[installation receipt](remaining_refinement_20260928/installation.json) records the installed hashes and checks.

This directory contains tooling and generated comparison assets for experimenting with MM `.fnt` bitmap-font upscales.

Current sample:

```sh
python3 tools/font_upscale/unpack_xbrz_font.py \
    assets_dev/engine/fonts/english_text/Arrus.fnt \
    tools/font_upscale/arrus \
    --scale 4
```

The script unpacks the source font glyph-by-glyph, runs `xbrz` on each glyph with transparent padding, then assembles
comparison sheets:

- `Arrus_sheet_original.png`
- `Arrus_sheet_x4_nearest.png`
- `Arrus_sheet_x4_xbrz.png`
- `Arrus_compare_x4.png`
