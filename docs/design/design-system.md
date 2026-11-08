# RiffCatch Design System

## Authority and status

The user-approved redesign target is [RiffCatch Retro-Tech DAW Interface](../screenshots/RiffCatch%20Retro-Tech%20DAW%20Interface.png). Open the image, not just this description, before visual design/review. If it is unavailable, ask the user to provide it. Existing app screenshots are baseline evidence, not replacement targets.

This document is a specification, not a claim that the target has shipped. At framework setup, `main` is v0.63: mostly flat themed controls, dark frames, lighter well rings, cached card shadows and blue seven-segment displays. There are seven built-in themes with Sunset as default. The discarded redesign is not an approved baseline. No GUI changes are authorized by installing this framework.

## Visual language

Neo-Retro audio hardware: dark navy/charcoal materials, tactile controls, recessed black working areas, warm orange/gold emphasis and cool blue illumination. Preserve the compact DAW layout and all current operations. This is not a generic flat dashboard and not a uniform neon outline treatment.

The reference's depth comes from deliberate light/dark contrast (Photoshop-style dodge and burn): crisp highlights on exposed edges, small reflected highlights below them, softer light on faces, dark bevel separation and contact shadows. Use consistent lighting rather than merely making every panel brighter.

## Reference observations

| Area | Visible target and acceptance intent |
| --- | --- |
| Toolbar | Rounded dark enclosure, pale-blue upper rim, localized bright glints and faint reflections below the top edge. Cool blue light across the upper surface, warmer light toward the lower-left edge. Do not flatten this into a single orange border or evenly spaced blue stripes. |
| Controls | Raised dark buttons inside deep frames, bright upper bevels and darker lower/contact edges. Blue tool accents; pink/red record, green play and orange loop remain semantically distinct. |
| Displays | Blue luminous seven-segment digits in dark blue recessed housings, bright bevels and controlled glow. Preserve the existing clock and drum/bass counter identity. |
| Tracks | Orange first track and warm yellow second track; localized colored edge light and bright name plates. Black timeline wells with layered light rims and dark inner edges, readable grid and content. |
| Knobs/faders | Dark metallic knobs with small highlights, shadows, readable pointers and colored value arcs. Fader caps stand apart from dark slots and scales. |
| Tabs/sequencer | Active tabs catch orange top light. The shown Bass panel has a warm rim, dark controls and a recessed piano roll with restrained row/grid contrast. Drums is not expanded in this image: apply shared principles without inventing hidden details. |
| Tuner/master | Inset dark tuner with cool edge reflections and legible scale; master has a warm title area, recessed meter/fader and clear numeric readout. |
| Workspace | Dark navy/black background with subtle texture, broad diagonal curved bands and localized warm light near the sides/bottom. Decorative detail remains behind and quieter than controls. |

## Color and typography

- Treat orange/gold as primary emphasis, blue as display/tool/reflection accent, charcoal as material and near-black as recess. Preserve record/play/warning semantics rather than forcing everything into the accent color.
- Exact target hex values, font family and effect dimensions are not measured here. Do not promote values from the discarded attempt to approved tokens. Sample or propose them explicitly during a scoped implementation.
- Route styling through `Theme` roles and existing `RiffLookAndFeel`; preserve all built-in/custom theme behavior. Current names are historical: `Theme::accent` maps to Primary, `Theme::cyan` to the preset Accent and `Theme::purple` to Secondary, not necessarily those literal hues.
- Keep strong transport/value hierarchy, readable small labels, stable numeric alignment and consistent icon strokes. Do not reproduce truncated mockup labels at the expense of usability.

## Material construction

- Reuse `Theme::framedTile`, `Theme::wellRing`, `Theme::cardShadow`, `Theme::radius` and `Theme::radiusLg` as implementation starting points, not as evidence that the target is already achieved.
- Typical paint order: outer/contact shadow, base surface, inset/bevel shading, localized reflected light, crisp exposed-edge highlight, content. Check actual compositing so later fills or borders do not erase highlights.
- Bright edges need dark neighbors to read as depth. Glows should taper and vary with position; avoid broad translucent rectangles, clipped corners, banding and blown-out labels.
- Normal, hover, pressed, toggled, disabled and keyboard-focused states must be distinguishable. A static reference does not specify all states; implement and verify them deliberately without altering behavior.
- Keep effects bounded and efficient during playback. Avoid repeated expensive blur work and new UI/DSP coupling.

## Scope and acceptance

For unrelated changes preserve the current visual language. For approved redesign tasks, this target supersedes the historical flat/no-gradient restriction. Implement one coherent scope at a time; do not remove features, change theme defaults or modify sound to match a picture.

### v0.80 / v0.81 two-element lighting prototype (not user-accepted)

The user authorized an iterative **Sub knob and Bass outer frame only** POC.
For these two elements only, explicit soft outside bloom supersedes the earlier
no-blur/no-separate-halo restrictions below. This does **not** authorize the
removed reflection under the Bass tab label, a generic rendering framework or
rollout to the other seven sound knobs, volume, tabs, Drums or tuner/master.

v0.80 introduced three padded native Gaussian alpha masks per emitter, with
current-snapshot caches rather than a per-value map. The independent visual
review rejected its scarcely visible Sub halo and narrow/faint frame tail.
v0.81 increases broad/medium halo energy and range, not the crisp 4px stripe
or 2.6px arc widths. Only Sub's same-geometry pointer is slightly brighter.
The native parent ring -> glow -> child face order and caption fade remain;
tab corners/gap, functional content and neighboring controls are protected.
All layout, controls, values/callbacks, persistence and audio remain unchanged.

These are local prototype constants, not approved global design tokens:
Windows native API radii (three times sigma) are frame 16.5/7.5/2.1 and
Sub 12/7.5/2 logical pixels; broad/medium/near opacities are .65/.50/.34
and .72/.65/.44 respectively, before existing Sub state modulation.
Color/state paints reuse masks; size/DPI and Sub value invalidate the relevant
snapshot. No new timers or audio-thread work. Sub parent invalidation includes
the union of old/new halo extents. Bounded mask-padding failure must log an
explicit diagnostic and retain failed status, not silently report success.

Coverage requires native-size normal/minimum Neo-Retro HWND captures, fresh
Release/selftest, out-of-shape decaying light, transparent mask borders, no
caption/tab/neighbor spill, geometry/state/zero-arc and silent-repaint checks,
and cache-cost measurements. Synthetic 144-DPI/theme/state fixtures are not
actual GUI interaction or alternate-monitor evidence. Visual inspection and
user acceptance remain separate; **no rollout before explicit POC approval**.

### v0.88 flat VU meters and material fader caps

User-selected scope: the VU meters, the master fader cap and the track
volume/pan drag circles. `LevelMeter` is flat again: one dark well fill, no
burned-in scale, no cylindrical gradient, no zone colours and no white
highlight. The bar is a flat Tokyo Night lilac `#BB9AF7` in every theme, with a
small lilac glow at the leading edge; peak-hold is a crisp 2px Tokyo blue
`#7AA2F7` line in every theme. Meter smoothing, peak timing, dB mapping and
component bounds are unchanged. The master fader keeps its 14px cap and travel
but now uses the material knob face, contact ring, accent ring and glint. Track
volume/pan knobs gain a defined inner grip ring inside the existing face, so
the drag circle reads more clearly without changing the face, outer ring, arc,
pointer or hit area. Non-master linear faders and all non-track knobs keep their
previous paint. Verification: Release selftest 1802 PASS / 0 FAIL, including a
new fixed-colour meter check across Tokyo Night, Sunset and Neo-Retro, plus
1500x980 and 1100x620 captures with active meters.

### v0.87 meters, tuner glow, pattern rail and recessed displays

User-selected scope: VU effects, tuner note glow, the Song button, the pattern
tab rail and all seven-segment housings. Meter behaviour, smoothing, peak hold,
zones and dB mapping are unchanged; `LevelMeter` now draws a recessed well with
faint unlit ticks, cylindrical zone gradients, a bounded leading-edge bloom and a
soft peak-hold marker. The tuner keeps its green in-tune state and uses orange
out of tune, with a note-coloured backlight and text halo clipped to the well;
the note letter also gets a thin black outline. The Song button opts into the
existing `toolbarControl` material. All pattern and Drums/Bass tabs share one
material (`Theme::tabFace`): the same flat face and dark contact line for active
and inactive tabs, a crisp rim in the tab colour on the active tab and grey on
inactive tabs, plus one localized glint on the active tab only. The active
Drums tab uses orange and the active Bass tab its ribbon colour. The `+` has no
solid fill and a grey outline, with the accent reserved for the glyph. The rail
now hugs its tabs and ends exactly at the `+` right edge in a rounded cap instead
of running as an empty strip into the Song button with hard corners. The tuner
rim glint follows the live theme accent instead of the fixed cool mixer light.
Track close buttons get a recessed dark tile, a slightly brighter cross and a
subtle outline. The seven-segment housing is now a matte dark-blue
recessed bezel with a shadowed upper lip and blue reflected light on the lower
lip; `Seg::digitColour` stays blue in every theme, the clock's separate raised
shadow/bloom is removed, and the Drums/Bass library counters use the same
brightness. `Theme::panelTopGlints` scales its glint count with panel width
instead of always drawing 8, so the mixer uses 3 and narrow cards stay clean;
the wide Drums/Bass panels cap at 5 and their glints follow the active tab
colour (orange for Drums, the Bass ribbon colour for Bass).

No geometry, hit area, callback, shortcut, transport state, parameter
range/default, undo, persistence, audio or display-digit colour changes. The
tuner selftest expectation is updated from accent to orange for the out-of-tune
marker, and the armed track-rim check scans for the strongest red glint instead
of a fixed position. Verification covers Neo-Retro, Sunset and Tokyo Night at
1500x980 and 1100x620, active meters, pattern tabs with `+`, Song on/off, clock
and library counters, plus a fresh Release selftest. No live input signal was
available for a window capture of the in-tune/out-of-tune tuner backlight; those
states are covered by the tuner selftest and offscreen paint checks.

Follow-up in the same iteration: Drums now uses the live **Primary** role and
Bass the live **Secondary** role for tab, enclosure and identity glints, so both
panels follow the selected theme instead of fixed orange/cyan. Bass's left
ribbon and tab ribbon both use Secondary, and the tuner/mixer rim glints use the
same Secondary ribbon colour. The Drums/Bass tab row is translated by `(1,1)` so
the active tab's bottom edge overlaps the 1px-inset panel frame and closes the
visible seam; the panel geometry and hit areas are unchanged. Neo-Retro is now
the eighth built-in preset and the default theme; a user theme JSON with a
built-in name is skipped, and a removed saved theme falls back to Neo-Retro.
Verification: Release selftest 1796 PASS / 0 FAIL, plus 1500x980 and 1100x620
captures with Drums and Bass active and 8x tab/panel and 4x/6x tuner crops.

### v0.86 exposed panel edges

User-selected scope: the four outer panel frames only, including the Bass frame.
Track cards, Drums, Mixer and Bass reuse the v0.85 toolbar material instead of a
uniform accent outline: a pale upper rim that fades before the lower half, a
dark inner neighbour, localized top glints and a faint reflection band. Drums'
orange 1px outline and Mixer's purple outline are replaced by that directional
edge; their accent stripes, tabs, titles, content and the Bass warm focus core
remain. Track cards keep their semantic track-colour rim, including the red
armed state, now with a slightly stronger exposed top contour and glints.

`Theme::panelExposedEdge` is the shared helper and calls
`Theme::panelTopGlints`; `toolbarEnclosure` uses the same glint function. Rim and
contact colours are live theme roles per panel (`trackRimLight`/`trackRimShade`,
`materialCoolLight`/`materialContact`, `mixerCoolLight`/`mixerContact`,
`bassFocusCore`/`bassControlContact`). No geometry, hit area, callback,
shortcut, transport state, parameter range/default, undo, persistence, audio or
display painting changes. The selftest keeps its track-rim checks on the
semantic right edge and the armed red glint peak.

Verification covers Neo-Retro, Sunset and Tokyo Night at 1500x980 with Drums and
Bass expanded, an armed track, Neo-Retro at 1100x620 and 2x/4x/6x edge crops,
plus a fresh Release selftest. The reference shows no recorded-clip state and no
hover/pressed panel frames, so those remain stated coverage gaps.

### v0.85 toolbar and transport material

Toolbar only, including the settings row. The merged top panel becomes a
rounded enclosure with a pale cool upper rim, localized glints along the exposed
top edge, a fainter reflection beneath it, a dark inner neighbour and a deeper
settings row. Transport `IconButton`s, `ToolButton`s, text buttons and combo
boxes opt into the existing material tile via `toolbarControl`, with an outside
contact shadow from the parent. The BPM readout becomes a recessed dark well and
the clock display housing gets a bounded contact shadow and cool rim bloom;
segment colours, text drawing and the display's own paint are unchanged.

No geometry, hit area, callback, shortcut, transport state, parameter
range/default, undo, persistence or audio behaviour changes. Roles are live
theme colours, so all built-ins and custom palettes are supported. Verification
covers the enclosure rim/glints/row separation, BPM well, transport and tool
opt-in, the full toolbar at 1500x980 and 1100x620 in Sunset, Tokyo Night and
Neo-Retro, and a fresh Release selftest. Hover/pressed states are verified only
offscreen, not by screenshot.

### v0.84 track panel material completion

Tracks only. The card and header block get a restrained top-to-bottom face
shading; the header controls and take/Hits controls opt into the existing
`trackControl` material used by Drums/Bass; pan and volume opt into the
existing `trackKnob` metallic face and value arc; the input meter gets a
narrow recessed housing; the grip dots get a light/contact pair. The v0.82 rim
and raised nameplate stay unchanged, as do clip body, waveform, grid, ruler,
selection and playhead painting.

No geometry, hit area, callback, parameter range/default, shortcut, undo,
persistence or audio behaviour changes. The material roles are live theme
roles, so built-in and custom palettes keep their behaviour. Verification uses
all seven built-in themes plus Neo-Retro at 1500x980, Sunset and Tokyo Night
at 1100x620 with empty, recorded and armed rows, a fresh Release/selftest and
the visual checklist. The reference has no recorded-clip state, so clip-body
material remains a stated coverage gap rather than an invented target.

### v0.83 built-in theme set

Ocean and Midnight are removed; seven built-ins remain in this order: Tokyo
Night, Forest, Sunset, Rose, Lavender, Coral and Mint. Sunset stays the
default. Saved theme selections continue to resolve by name, so custom themes
and unaffected built-ins keep their selection; a saved name that no longer
exists falls back to Sunset instead of reusing a now-shifted numeric index.
Theme cycling, the theme menu separator and custom-theme storage use the
updated built-in count. Verification checks the full cycle and a Release
selftest.

### v0.82 track rim and raised nameplate

Only each track card's outer rim and its nameplate change; the v0.81 Bass POC
is untouched. Geometry, `waveLeft=394`, `clipStripH=14`, hit areas, meters,
take controls, tick-driven repaint and every callback remain unchanged.

The rim is a bounded directional treatment in the live track colour, not a
uniform neon outline: a crisp top glint fading before both rounded corners, a
short upper-left return, a faint upper-right return and a low-alpha full
contour. A dark rounded neighbour sits immediately inside the bright edge.
Armed and recording cards keep the semantic red through the existing `glow`
choice; selected cards retain their accent outline.

The nameplate keeps its existing bounds and text colour but becomes a raised
face: a dark contact shadow below, a top-to-bottom track-colour gradient,
a crisp top highlight with a short reflected light beneath it, and a thin dark
lower contact edge. All effects are layered fills/strokes clipped to the
existing plate and card; no blur, bloom, image cache or new timer is added.
Palette roles are derived from the live track colour, so custom and built-in
themes retain their behaviour. Verification uses Sunset and Tokyo Night at
1500x980 and 1100x620 with empty, recorded and armed rows, plus a fresh
Release/selftest and the visual checklist. User acceptance remains separate.

### v0.65 implementation scope

Only Bass's Sub, Cutoff, Reso, Env, Decay, Sustain, Glide and Drive opt into the
`bassSoundKnob` face in `RiffLookAndFeel::drawRotarySlider`. Live `Theme::bassKnob*`
roles provide restrained upper-left metallic light, a tapering upper-edge reflection,
a narrow lower contact bevel and moderately stronger captions. Existing outer rings,
value arcs/pointers, bounds, label typography and parameter bindings are retained;
unmarked sliders (including Bass volume) keep the legacy paint path. No blur, palette
defaults, DSP or v0.64 enclosure/tab/piano-roll changes are included. Before/after
comparisons and interaction checks complement the headless regressions; user acceptance
remains separate.

### v0.66 implementation scope

Bass's seven header/sound combos, Random, Rec/Wait/REC, Bake, M/S, Clear,
Echo/Rev and page buttons opt into `bassControl`. The Bass library counter
opts in only its arrow tiles, not its blue display. Live `bassControlLight`
and `bassControlContact` roles add restrained upper-left light, a narrow
reflection and a lower contact edge inside the existing dark frames.
Semantic active fills, record red and focus policy are retained. The volume
face explicitly opts into `bassVolumeKnob` using the existing knob material;
the eight v0.65 sound knobs are unchanged. `bassNoteLabel` strengthens only
the small piano-roll note names, not the grid surface.

Below 1000px Bass-panel width (1134px with page navigation visible), the
header wraps into two 28px control rows, adding 32px locally above Bass.
Library/Bars/Root/Scale and pages occupy the first row; Template/Chords/Random
and recording/mix actions occupy the second. The parent reserves this extra
height only while Bass is selected; mixer and Drums retain their positions.
The 143px note grid, sound-control geometry, typography and input mapping are
retained, offset together by the extra header height. The passive header
meter remains omitted when space is tight, as before; no action is hidden.
Normal unpaged 1500x980 window layout remains on the original single row.
Automated bounds/paint/parameter checks complement actual 1100x620 and
1500x980 captures. Visual theme verification for this scope is restricted to
Sunset and Tokyo Night; user acceptance remains separate.

### v0.67 implementation scope

Bass-only dodge/burn paint continuation; all v0.66 layout, opt-ins and behavior
remain unchanged. `bassEnclosure` retains the live `bassRim` contour at a quieter
far edge, with a bounded warm upper-left focus (65% toward the existing orange
role), an inset, fully visible 1.5px local bright core and an adjacent dark contact bevel.
The active Bass tab alone receives the same material with shorter top light.
The halo fades to zero rather than tracing a uniform neon box.

`bassControlTile` uses a low-saturation, high-value exposed-metal role rather
than brightening the dark raised surface. A short top-face reflection, crisp
tapered upper rim, dark lower contact and short lower reflection preserve
semantic fills, record red and content. Hover light is 15% stronger; pressed
light is halved and disabled light is 25% of normal. Library arrows omit the
face wash. Blue displays, volume face and all eight sound-knob faces are unchanged.
`bassWellRim` centralizes only the existing 3px piano-roll surround, pairing a
cool bright top rim with a dark inner edge; no grid overlay or geometry change.
No global palette/helper, toolbar, mixer or Drums restyling is included.
Visual comparison is scoped to Sunset/default and Tokyo Night at 100% scaling,
1500x980 and 1100x620; acceptance remains subject to actual captures and review.

### v0.68 implementation scope

`Resources/themes/Neo-Retro.json` is an optional, shareable custom theme, not a
tenth built-in. To install, copy that file to `%APPDATA%\RiffCatch\themes\`
(create the folder if needed), without overwriting an existing user theme of
the same name. Restart RiffCatch and select **Theme > Neo-Retro**. Sunset remains
the default; installation does not change the saved selection.

The six opaque roles are Primary `#FF780A`, Secondary `#F2AB24`, Accent
`#F6C343`, Background `#0B1018`, Surface `#202630`, Text `#DCE3EC`.
These are approved visual proposals for a navy/charcoal base and warm orange/gold
details, not sampled reference colours. Existing mapping is unchanged:
`blue = accent = Primary`, `purple = Secondary`, `cyan = Accent`.
Record/play colours and the separate blue seven-segment displays remain fixed.

Only the Bass enclosure and active Bass tab change material geometry. Explicit
rounded bands replace the broad centred contact/halo strokes: the 6px outer
radius remains, and the halo's inner contour retains at least a 3px radius
instead of collapsing to a square. Nested half-pixel bands fade the inner halo
while retaining v0.67's directional hotspot, peak light and dark contact bevel.
Physical bounds, core highlight, other paints and all v0.66 layout stay unchanged.
Capture coverage adds Neo-Retro to Sunset/Tokyo Night, at normal/minimum size.

### v0.69 implementation scope

Only the Bass panel's short lower reflection changes: it starts transparent,
ramps into the existing warm highlight and fades out along the same bounds.
This removes the abrupt dark-to-light start just after the lower-left curve.
The v0.68 rounded bands, tab, palette, controls and layout are unchanged.

### v0.70 implementation scope

Bass control highlights follow a closed, inset rounded contour with light
fading along the top and down the left side, instead of an open bright stroke
ending just after the corner. The short lower reflection also fades in.
Control bounds, semantic fills, input, palette and all non-Bass paint are unchanged.

### v0.71 implementation scope

Bass-only ribbon/glow continuation, with no layout, input, persistence or DSP
changes. The panel retains its v0.68 rounded bands and v0.69 lower reflection
ramp, adding a sustained gold left core (1.5px) and bounded inward halo through
the full height, rather than amplifying only the old 70px hotspot. The mid/lower
core targets roughly 70% of upper brightness; other panel edges remain subdued.
The active Bass tab has a near-full-width orange top core with inward falloff
and a faint 4px-high, 45%-width radial reflection beneath its caption.
Both active and inactive Bass captions receive a thin black glyph contour;
font, placement, data colour/dot and Drums painting are retained.

Existing `bassControl` buttons/combos get parent-painted, coordinate-converted
shadows at unchanged bounds; library arrows paint their shadows locally without
changing the blue display. Four coverage layers give a 1.5px downward offset
and 2.25px falloff, weaker when pressed/disabled. State changes invalidate only
the bounded parent shadow area, with no new repaint timer. The v0.70 closed
control highlights are unchanged.

All eight Bass sound knobs and Bass volume retain their faces, pointers and
2.6px value arcs, adding slight downward ellipse shadows, 6px/4px low-alpha
family-colour halos and a 1px brighter arc core. Glow follows the actual arc,
including a truly empty zero-length arc. All roles are live and Bass-only;
no global palette changes, blur passes or new image buffers are introduced.
Actual window evidence and separate visual review remain required; headless
paint/parameter regressions are not user acceptance.

### v0.72 correction scope

Only the three v0.71 lighting review failures are corrected. The sustained
left ribbon keeps its core/contact geometry but uses a single inward light
field fading over 14px, instead of multiplying a narrow band mask by another
fade. The active Bass tab uses a continuous 10px top-light falloff and a
localized 7px-high soft reflection below the caption, not a surface wash.
The value-arc halo now tapers through 9–4px coverage strokes outside the
unchanged metallic face, leaving visible family-coloured light beyond the
2.6px solid arc; the pale core is quieter. No blur/image cache is added.
The v0.71 shadows and glyph contours, rounded joins, all physical geometry,
controls, blue displays, themes, persistence and audio behavior are retained.
Related regressions measure light outside the solid cores and contrast after
compositing the reflection onto the actual tab face. Native-size Neo-Retro
normal/minimum window captures remain required; other GUI themes/states/DPI
and user acceptance are not implied by these offscreen tests.

### v0.73 explicit correction scope

The Bass panel's attenuated left ribbon/halo is replaced by the tuner/master
card's solid 4px fill pattern, clipped to the existing radius-6 enclosure.
Bass retains its 1px enclosure inset and uses the unattenuated live Accent
role (`cyan`, yellow in Neo-Retro), not the master's Secondary colour.
The separate reflection below the Bass tab label is removed entirely.
The orange tab top, thin black glyph contour, knob glow, control shadows,
buttons and smooth v0.69 panel-bottom reflection remain unchanged.
No bounds, layout, tuner painting, themes/settings, interactions or DSP change.
Regressions check full-height opaque stripe pixels, width/rounded clipping,
absence of the separate left halo and absence of the under-label reflection
in actual `SeqTabs` painting. Build and window verification remain separate.

### v0.74 Bass endpoints / first tuner-master material pass

Bass keeps the opaque live Accent stripe at x+1 and 4px width, with radius-2
inner end corners tangent to the straight side, clipped by the existing
radius-6 enclosure. Only the endpoints change; no ribbon halo, detached cap,
horizontal stripe or under-caption reflection is added. All other Bass
lighting and geometry remain as in v0.73.

Only the tuner/master enclosure, the tuner's existing 3px surround, and the
master strip receive modest directional upper-left light and dark contact
edges. New live `mixerCoolLight` is explicitly cool, independent of the
historical Accent/cyan role (gold in Neo-Retro). The panel base, Secondary
stripe, existing single master shadow, 22px header and all well/readout bounds
are retained. No tuning text/markers are overpainted.

Only the master output slider sets `masterOutputFader`: its existing 14px
round cap gets a light metallic face and inset contact edge. Slot, value line,
travel and legacy painting for every unmarked slider are unchanged.
Shared `framedTile`, `wellRing`, `cardShadow` and LevelMeter remain unchanged.
No resize, focus, reference/menu, gain/undo, persistence, timer or DSP changes.
Offscreen regressions check all built-in palettes plus Neo-Retro fader paint,
real component bounds/callbacks and tuner paint fed through its real detector;
these are not manual audio or GUI interaction acceptance.

### v0.75 equal bottom-row frames / subtle shadows

The tuner/master frame follows the selected expanded sequencer's actual painted top
and bottom, including Bass's 32px wrapped header and page-navigation breakpoint.
Bass and mixer retain a shared 1px painted inset; the unchanged edge-painted
Drums frame is accounted for when positioning the mixer. Tab switches, page
visibility, collapse and resize use the same bottom-row layout. Bass's header,
143px piano roll, control positions and the track-area reservation are retained.

Equal-height frames apply only to an expanded sequencer. When folded, the left
enclosure retains its established preferred header height (Drums 42px, Bass
42/74px); tuner/master stays full-size at the original fixed bottom-row position,
with its 1px painted inset. No empty left frame is retained and no tuner controls
are compressed. The existing track-area reservation is unchanged.
The right-hand children use their existing resize logic (including real master
fader travel); normal expanded Bass still gives a 206px mixer, wrapped Bass 238px.

The single cached outer mixer shadow is parent-painted at the visible frame,
not clipped to the component. The tuner gets a separate, low-opacity downward
shadow with a bounded 3.25px extent that fits existing padding without reducing
hit areas. The existing master-only shadow is retained once. No shared shadow
helper, v0.74 material/stripes, tuner reference/detection, gain/undo, themes,
settings or DSP changes. Automated checks exercise production row layout and
painted-edge equality, folded/expanded/tab/page/width cases, shadow extent and
actual resized child bounds, hit targets and value mapping. Folded cases check
preferred left heights and the intact normal mixer separately, not equal edges.
Native Neo-Retro
window evidence and manual interaction/audio acceptance remain separate.

### v0.76 collapse regression correction

Restores the pre-v0.75 preferred-height collapse of Bass/Drums. The v0.75
expanded painted-edge alignment, normal/wrapped geometry, all shadows and
materials remain unchanged. Only expanded sequencers align both mixer edges;
folding preserves the original full-size mixer position and track reservation.

### v0.77 first Drums material pass (reduced user scope)

Only the Drums panel, active tab and its own controls. Tracks, main panel and
workspace were explicitly deferred by the user. Bass v0.76 (including
the opaque 4px yellow stripe, rounded inner ends, tab glyph contour, knob glow
and shadows), tuner/master, and `layoutBottomRow` are frozen. The removed
reflection below the Bass caption must not return. All controls, bounds,
callbacks, ranges/defaults, shortcuts, undo, persistence and audio stay intact;
the known minimum-width toolbar overlap is not a layout task in this pass.

Drums retains its edge-painted enclosure (not Bass's 1px inset), orange 4px
stripe and six original lane/cell/accent/repeat/playhead layers. Its dark
upper-left material, rounded stripe ends and orange active-tab top light
derive from the established material; the reference does not show expanded
Drums. `drumControl` explicitly opts header controls/library arrows into the
unchanged Bass tactile helper. `drumKnob` adds the existing metallic face and
actual value-arc halo, tighter/weaker on the 26px lane knobs.

Live `materialCoolLight` is separately named, never inferred from `cyan`
(gold in Neo-Retro). No grain, blur, image buffer, repaint timer, new
component/framework/dependency or DSP coupling is introduced.
Shared default `framedTile`, `wellRing`, `cardShadow`, LevelMeter and unmarked
controls remain unchanged. Drums well lighting stays strictly outside content.
Production paint/state/opt-in regressions supplement, not replace, genuine
normal/minimum HWND captures and manual GUI/audio acceptance.

### v0.78 expanded tab-switch stability correction

The user explicitly approved a shared bottom-row height correction after noticing
movement when switching Drums/Bass. Expanded layout now reserves one budget,
independent of the selected tab: the maximum of Bass's width/page-dependent
preferred height and Drums' preferred height plus the two pixels needed for the
Bass/mixer painted inset. This gives a 208px component budget normally and 238px
when Bass wraps, including when the paged Bass is hidden behind Drums.

Both tab strips have identical bounds. Bass and mixer use the common component
top/bottom; edge-painted Drums occupies the common inset frame, aligning all
visible contours. Thus tab switches change neither the tab baseline, visible
frame, mixer bounds nor the track viewport reservation. Drums' local controls
and 26px lanes, and Bass's local controls and fixed 143px grid, are not stretched;
unused height stays as enclosure space. Drums' local coordinate mapping is
unchanged; the parent accounts for the 1px frame inset in its bounds.

Folded mode retains the original 42px Drums and 42/74px Bass preferred heights
and original full-size mixer position/reservation behavior; no full-height empty
collapsed frame is introduced. All v0.77 material, controls, callbacks, ranges,
undo, settings and audio remain unchanged. Tracks, main panel and background
restyling remain deferred. Native regressions explicitly compare before/after
tab callbacks, width/page breakpoints, resize, disabled and collapse/expand
states. Genuine GUI tab-switch evidence remains a separate requirement.

### v0.79 all shared sequencer header anchors

The user approved reserving Bass-only slots in Drums, rather than aligning only
M/S. One small `SeqHeaderLayout` geometry value object now places chevron,
library counter, Bars, Template, page arrows/label, Clear, M/S and volume.
Their native parent-coordinate bounds and dimensions are identical. Drums leaves
Root/Scale/Chords/Random/Rec/Bake slots empty; no Bass feature is removed.
The logical layout retains Bass's existing allocation and 1000/1134px wrap
thresholds, but uses the union of **both panels' actual paging visibility**.
Each panel retains its own page visibility, enabled states and callbacks.

The parent converts logical coordinates using the actual panel-origin difference.
Bass keeps its 1px painted inset and Drums remains edge-painted; no material,
glow, stripe or tab effect is changed to conceal a geometry discrepancy.
Wrapped Drums uses the shared 74px header. Its entire lane paint, knobs,
mute/audition and step hit mapping move together; lanes remain 26px and the Bass
grid remains 143px. Expanded enclosure budgets are now 208/240px, with stable
painted contours, tabs, mixer and remaining track rectangle across tab changes.

Collapse remains **header-only with the original full-size mixer**, not an empty
expanded frame. Necessarily, folded Drums now shares Bass's 42/74px logical
header budget as well, and inset-coordinate conversion gives a 40/72px Drums
component. Folded tabs and reservation no longer depend on which pane is selected.
This is the bounded consequence of aligning every common control even when
folded; it supersedes v0.78's separate folded header heights.

No tracks/main/background restyling, DSP, controls, defaults, ranges, shortcuts,
undo semantics, project/settings schema or Bass/tuner material changes.
Native regressions compare actual production control bounds via a common parent,
all four paging combinations, normal/current/minimum sizes and both sides of
the wrap thresholds, tab callbacks, resize, disabled and collapse/expand states.
Real component gestures exercise shifted Drum lanes, accents, drag/repeats/pages,
mute/audition and undo/redo, plus Bass note/length/accent/slide/page mapping.
Synthetic tests are not GUI/audio acceptance; genuine HWND evidence and manual
interaction remain separately recorded.

### v0.97 flat pattern rail and song blocks

The pattern rail and the song row now use a flat browser-style pill material
(`Theme::flatTabFace`, `Theme::tabColourChip`, `Theme::circleClose`,
`Theme::flatPlusTile`): one solid face, no rim, gradient or glint, a small
pattern-colour chip, an x inside a filled circle for close (red on hover) and a
flat "+" tile with no outline. The Drums/Bass tabs keep `Theme::tabFace`.
Song rearrange now lifts the block out of the row: a dashed accent slot marks the
landing position and a floating copy follows the pointer; the order is committed
on release instead of reordering live. The empty-song hint starts after the flat
"+" tile so the two never overlap. Tab/block geometry, close hit areas,
dialogs, wheel repeats, menus and the tab rail are unchanged apart from the new
close and plus visuals. No DSP, undo, shortcut, default or project/settings
schema changes.

Completion requires the [visual checklist](visual-checklist.md), a successful build/selftest for code changes, actual screenshot comparison and user testing. Missing visual evidence means incomplete verification. Commit and push remain separate user-approved steps.
