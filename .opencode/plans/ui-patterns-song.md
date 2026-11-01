# UI round + Pattern / Song mode

Order of work: 1-3 are small UI tasks (v0.54), pattern mode (v0.55), song mode (v0.56). Each step is independently useful and testable.

## 1. Track-row volume as a knob (v0.54)
- `TrackRow` header: replace the horizontal `fader` (+ dB label) with a rotary knob like the pan knob (same style, accent colour, "uni" arc from the left, double-click = 0 dB, popup / label shows dB while turning). Keeps `faderToGain` mapping and the existing undo (`commitVol`). Frees width in the header row for e.g. an input meter or send knobs later.
- The mixer keeps its vertical faders (see 3).

## 2. Drums | Bass as browser-style tabs (v0.54)
- Restyle `SeqTabs`: tab shapes with rounded top corners, the open tab in the panel colour and visually joined to the panel body, inactive tabs darker, slight overlap, a thin accent line on top of the open tab. Sits flush at the top-left edge of the panel (the panel header loses its separate title area). "Has data" is shown with a small dot or a tinted top line instead of a filled button.
- The same tab component drives the pattern tabs in step 4.

## 3. Mixer closer to Ableton (v0.54, sends optional)
From the screenshot: coloured header bar with the track name, a coloured number button that doubles as arm / select, S and a record dot underneath, volume knob with a dB read-out above, tall fader with the VU beside it and a dB scale (0, 12, 24, 36, 48, 60), pan knob, (Ableton: Audio From / Monitor / Audio To, Sends A/B).
- `ChannelStrip`: header in the track colour (`Theme::trackColour`), dB number box (double-click to type), fader + stereo VU + tick scale, number / arm button, M / S, record dot; echo / reverb stay as toggles for now.
- Optional phase 2: real sends. Two shared return buses (A = Reverb, B = Echo) with a send knob per track and level + mute on each return strip, replacing the per-track insert toggles. Needs engine work (return buses, TrackFx moves from track to bus) and a project-format migration; the old toggles map to a send level (on = fixed level).

## 4. Pattern mode (v0.55)
Tracker idea: up to 16 patterns, each a tab with name and colour.
- A pattern holds everything musical: audio clip of every track (offset, length, takes), drum pattern, bass pattern + synth settings, loop length in bars. Global per project: tempo, time signature, mixer (volume, pan, fx, names, inputs), key.
- UI: a tab strip above the ruler (16 slots, "+" to add, double-click to rename, right-click for colour / duplicate / clear / delete); the track area and the Drums / Bass panel always show the open pattern. Switching tabs stores the current pattern and loads the other one (buffers are swapped under the existing track locks, transport stopped or switched at the next loop start while playing).
- Engine: `Track` keeps its current single clip machinery (record, takes, edit, undo keep working). A `PatternBank` (message thread) stores the 16 frozen copies; the open pattern is the live one. Undo history is per project (a tab switch is not undoable, edits inside a pattern are).
- Project file: `patterns` array (name, colour, loopBars, per-track clip as WAV files `pattern03_track02.wav`, drums + bass JSON) plus `currentPattern`. Old projects load as pattern 1.

## 5. Song mode (v0.56)
- Button next to Play / Loop: Pattern | Song. Song view replaces the track area with an arrangement strip: a row of clips, each a pattern (coloured, named) with a length in repeats of that pattern's loop; drag from the pattern palette (the 16 tabs) onto the strip, drag to reorder, drag the right edge to repeat, right-click to duplicate / delete / split; click a clip to open that pattern for editing.
- Playback: on Play in Song mode the engine plays the clips in order; at each clip boundary it switches pattern at a sample-exact point (like a loop wrap). To stay real-time safe, Play first builds a frozen playback set (per pattern: audio buffers per track, drum and bass snapshots) on the message thread; the audio thread only switches an atomic index and re-applies the snapshots (no allocation, no locks). Edits while the song plays take effect after Stop (documented).
- Mixer, tempo and time signature are global. Optional later: per-clip track mute, clip transposition, song loop range, bounce song to audio (reuses the Exporter with the same pattern switching).
- Export: "Export mix" in Song mode renders the whole arrangement.

## Tests
- Pattern round trip through the project file, tab switch keeps audio / drums / bass, old project loads as pattern 1.
- Song playback: pattern A then B boundaries sample-exact (per-pattern test tones), repeats, empty song, export length = sum of clip lengths.
- UI tests for the new tab strip (select, rename) with synthetic mouse events like the bass panel.
