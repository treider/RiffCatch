# Bass synth (v0.49, part 1)

Decisions: piano-roll-like grid in a scale, tabs "Drums | Bass" in one panel slot, templates + per-bar chord progression + bake to track. MIDI keyboard recording is v0.50.

## Audio: `Source/audio/BassMachine.h` (new, mirrors DrumMachine)
- Cell = one atomic uint32 per step: degree+64 (bits 0-7, 0 = rest), length in steps (8-15), slide (bit 16), accent (bit 17). maxBars 8, maxStepsPerBar 20.
- Key: root E1..D#2 (midi 28-39), 7 scales (minor/major pentatonic, minor, major, blues, dorian, chromatic). Per-bar transposition in semitones `shift[bar]` (chord progression). Snapshot (undo) holds steps, shift, bars, barSteps, root, scale.
- Mono voice: saw/square (polyBLEP) or triangle, sub oscillator (-1 oct), two TPT state-variable low-pass filters in series (24 dB, resonance on the first), filter envelope (cutoff * 2^(env*4.5)), amp env (2.5 ms attack, decay to sustain, 20 ms release), glide, tanh drive.
- Knobs: Sub, Cutoff, Reso, Env, Decay, Sustain, Glide, Drive + wave combo. Presets: Sub, Pluck, Fat saw, Acid, Square.
- Sequencing as DrumMachine::process: absolute sample clock, ceil(k*stepLen), phase kept across loop passes. Gate = len*stepLen*0.85, or 1.0 when the next note is a slide (legato, glide only). `auditionNote` plays a 0.35 s preview.
- `AudioEngine`: member `bass`, `setTimeSignature` calls `bass.setBarSteps`, `processChunk` renders it after drums (solo logic includes `bass.solo`), reset on `drumReset`/`jumpTo`, `prepare` on sample-rate change.

## Templates: `Source/audio/BassTemplates.h` (new)
- 16-step note lists tiled over `barSteps` (works for every time signature), notes given in semitones and mapped with `nearestDegree`: Root pulse, Eighths, Root+octave, Root+fifth, Funk, Arpeggio, Walking, Acid slides.
- "Lock to kick": root on every kick of the drum pattern (length up to the next kick, max one beat), bars = drum bars.
- Progressions (shift per bar, cycled over 4 bars): None, I-IV-V-IV, I-V-vi-IV, I-vi-IV-V, i-bVI-bIII-bVII, i-bVII-bVI-V. If bars < 4, expand to 4 by copying bar 1.

## UI
- `ui/SeqTabs.h`: small "DRUMS | BASS" tab component; DrumPanel's title label is replaced by it.
- `ui/BassPanel.h/.cpp`: header (tabs, bars, root, scale, template, progression, page buttons, VU, Clear, Bake, M, S, volume); left column (394 px): sound combo, wave combo, 2x4 knobs; grid: 13 px bar-shift strip + 11 rows x 13 px (same 156 px as the drum lanes). Click = add note (len 1), drag right = length, drag up/down = pitch; click on a note head = delete; right click = accent; Ctrl+click = slide; clicking a bar label opens a menu of transpositions. Notes are previewed via `auditionNote`.
- `MainComponent`: `bassPanel` shares the slot with `drumPanel`; `showSeqTab(int)` (persisted as `seqTab`); collapse state is shared; forwards `setView`, `tick`, `refreshFromEngine`, wheel and undo; `newProject` calls `bass.resetAll()`; autosave considers `bass.hasAnySteps()`.
- `bakeBass()`: renders one loop (or the pattern length when the loop is off) offline to a new track "Bass" (like Catch), undoable, and mutes the bass machine so it does not play twice (undo restores).

## Project and export
- `ProjectIO`: `"bass"` object (bars, root, scale, volume, mute, solo, synth params, `steps` as comma-separated cell numbers, `shift` array), clamped on load like drums.
- `Exporter::renderMix`: passLen includes the bass pattern, bass rendered after drums with the same solo logic.

## Tests (SelfTest.h)
- Note timing and pitch (A root, degree 0 -> 55 Hz), length/gate, slide keeps the envelope, per-bar shift, 3/4 barSteps remap, export contains bass, project round-trip, bake length, templates (lock to kick positions).

## Housekeeping
- CMakeLists: add `Source/ui/BassPanel.cpp`, version 0.49. README: Bass section + shortcut notes.
