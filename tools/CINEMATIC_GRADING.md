# Cinematic color grading

Open **Escape → Controls → Video Options**. Toggle **Cinematic grading** and drag **Strength** from 0 to 100%.
Changes apply without restarting and are saved in `settings.ini`. Grading defaults to off; the initial strength
is 70%. At 70% the effect matches the previous version's 100%; the new 100% applies about 1.43 times that
color adjustment. The HUD, text, portraits, menus and debug console retain their original colors.

Equivalent settings:

```ini
[video]
cinematic_grading=true
cinematic_strength=70
```

A reproducible desktop preview:

```sh
./tools/run_game.sh --isolated cinematic --world mm6 --map oute3.odm \
  --set video.cinematic_grading=true --set video.cinematic_strength=70 --seconds 0
```

The shared world pass uses a 32×32×32 RGBA8 color lookup texture (128 KiB): lifted shadow/midtone detail,
selectively muted oranges/browns, and gentler green/blue desaturation. Neutral colors stay neutral and black/white
endpoints remain anchored. It grades the renderer's existing display-referred
SDR colors; it is not an HDR lighting conversion. It approximates the color atmosphere of the reference,
without neural reconstruction, new lighting, blur or altered geometry.

Only enabled, positive-strength gameplay frames are redirected into a window-sized color/depth target.
A fullscreen triangle samples that color and the LUT, then the ordinary HUD draws over it. Disabled and
zero-strength frames render directly to the original backbuffer. Resources are allocated on first use,
resized with the window and released with the renderer. OpenGL and Vulkan use the same grading code,
including the render-target origin correction.

Shader path resolution is shared with the existing indoor/outdoor shader loaders. The new shader binaries
are included in the normal CMake runtime shader list and package extraction metadata.

## Validation

- Normal OpenGL and separate Vulkan builds passed.
- Settings tests passed, including default-off and nondefault strength round-trip coverage.
- Desktop captures verified outdoor rendering in MM6/MM7/MM8, and MM6 indoor rendering.
- Native Video Options clicks changed On/Off and strength, persisted the settings, and returned to gameplay.
- Native resizing from 1280×720 to 960×540 and back preserved the image.
- In one running game, switching grading off and subsequently enabling it with strength zero restored the
  direct rendering path without stale frames or tinted HUD elements.

Local review images and isolated run records are under `build-vulkan/cinematic-review/`; that directory is
ignored build output. `lifecycle_options.png` shows the controls, and `lifecycle_resized.png` the resized game.
The Xvfb UI captures establish interaction behavior, not hardware performance.

## Desktop frame-rate check

RTX 3060 Ti, NVIDIA 595.99.02, MM6 New Sorpigal, 1280×720, VSync off, classic controls to hold the camera fixed,
strength 100%, three-second warmup and ten-second samples:

| Renderer | Off median FPS | On median FPS | Change in reciprocal median FPS |
| --- | ---: | ---: | ---: |
| OpenGL | 2981.91 | 2738.465 | +0.030 ms/frame |
| Vulkan | 3449.23 | 3426.215 | +0.002 ms/frame |

These are indicative whole-frame observations from one on/off pair, not isolated GPU timestamps or a guaranteed
cost across maps/resolutions. The Vulkan difference is too small to distinguish confidently from run-to-run noise.
At these very high uncapped rates the OpenGL difference is about 8% in FPS, despite the small absolute frame-time
change. Gold and food HUD rectangles were pixel-identical between the OpenGL on/off captures; sampled static
world colors changed on both backends as expected.

## Reference 1332 retune

The initial curve made shadows darker than the intended reference. The revised LUT lifts luminance through
the lower/middle tones, softens highlights and removes the warm/cool tint. Smooth hue weights reduce orange
roofs and earth more strongly than red foliage, with smaller reductions for greens and blues. The existing
strength control blends the original image with this new look.

Only LUT generation changed: the texture size, shader, texture fetch count and fullscreen pass are identical.
The timing figures above were measured with the initial LUT. New visual comparisons are stored under
`build-vulkan/cinematic-review/retune/`, separately from the original captures. The matched OpenGL captures
confirm less orange in the ground and lifted green shadow tones; sampled gold/food HUD rectangles remain
pixel-identical. Restart the game to load the revised LUT, then use the existing strength slider as usual.
