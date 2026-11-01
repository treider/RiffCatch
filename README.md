# RiffCatch

A lightweight multitrack looper and recorder for guitarists, built with JUCE and C++ for Windows. Plug in your interface (tested with a Focusrite 6i6 over ASIO), set a tempo, and layer riffs on top of a built-in drum machine.

![RiffCatch main window, Sunset theme](docs/screenshots/main-sunset.png)

## Features

### Recording and looping
- Up to 16 tracks, each with its own input selector, arm, mute, solo, volume and pan
- Loop lengths from 1 to 64 bars, sample-synchronous transport
- Time signatures 2/4, 3/4, 4/4, 5/4, 6/8 and 7/8 (BPM counts quarter notes; the click, ruler, loop length and drum grid follow)
- Count-in, metronome click with volume, tap tempo
- Punch in/out recording
- Undo/redo for editing actions
- Autosave with restore prompt after a crash

### Catch and looper
- **Catch** (button or Shift+C): the app always listens to the armed input and keeps the last 70 seconds in memory (nothing is written to disk). Catch saves the last 15, 30 or 60 seconds into a new track, with silence trimmed
- **Looper mode** (button): one key (Shift+Space) or a MIDI foot switch. First press records the first loop, second press closes it (rounded to whole bars, BPM follows), then each press records one pass on a new track starting at the next loop start. Learn the foot switch from File > Looper pedal

### Drum machine
- Step sequencer in sixteenth notes with Kick, Snare, Clap, Hat, Open hat and Cymbal. The bar layout follows the time signature (16 steps in 4/4, 12 in 3/4 and 6/8, 14 in 7/8, 20 in 5/4)
- Per-row volume and pan (pitch on Cymbal), master mute/solo and level meter
- Patterns of 1 to 8 bars, with accents and per-lane mute
- 21 ready-made two-bar grooves with a fill (Rock and Pop in 4/4, plus waltz, polka, march, 5/4, 6/8 and 7/8 patterns). The list shows the templates for the current time signature, and applying one is undoable

![Drum templates](docs/screenshots/drum-templates.png)

### Bass synth
- Mono bass synth with a step sequencer, in a tab next to the drums (Drums | Bass); a tab is filled with the primary colour when it holds a pattern
- Piano-roll style grid in a chosen key and scale (root E-D#, seven scales); click to add a note, drag right for length (drones up to 8 bars), drag up/down for pitch, drag the right edge of a note to resize it, click a note to remove it, drag across notes to wipe them, right-click = accent, Ctrl+click = slide
- Sound: saw / square / triangle, sub oscillator, 24 dB low-pass with resonance and envelope, decay, sustain, glide and drive; five presets (Sub, Pluck, Fat saw, Acid, Square); per-bass echo and reverb
- Chromatic scale shows every note (13 rows, C2-C3 with root C; Oct-/Oct+ move the window); Random generates notes inside the scale on a 1/4, 1/8 or 1/16 grid with density, random lengths, range and repeat options (right-click rolls again)
- Templates for every time signature, including "Lock to kick" (root note on every kick of the drum pattern), and chord progressions that transpose the pattern bar by bar
- Bake renders the line to a normal audio track; Rec records one loop pass live (with count-in) so knob moves such as Cutoff are captured as audio
- Pattern, key and sound are saved in the project and included in the export
### Riff analysis
- Right-click a recorded clip and choose "Analyze riff and suggest bass..." (no AI involved, plain signal analysis)
- Finds the tempo, the key and scale or mode (minor and major, pentatonic, blues, Dorian, Phrygian, Lydian, Mixolydian) with alternatives to pick from, one chord per bar, and the rhythm of the riff on the 1/16 grid
- Suggests bass lines that follow the chords and the riff: Root follow, Riff rhythm, Pedal, Walking, Octave pulse, and Lock to kick when the drums have a kick; Use puts one in the Bass tab (undoable), and you can try the next one
- Best on a clean single riff or power chords; heavy distortion or dense chords can fool the chord and mode guess, so check the key and chords shown
### Editing
- Waveform editing with selection, crop, cut, copy, paste, fades and reverse
- Right-drag for fine clip positioning, drag the title strip to move a clip
- Ctrl+drag the title strip to duplicate a clip (snaps to bars, loop grows automatically)
- Snap to beats, bars or 1/16 with modifier keys
- Zoom with Ctrl+wheel, pan with Shift+wheel
- Trim silence (clip menu) keeps the timing. Imported files are trimmed automatically
- Import audio files (WAV and others) onto a new track by menu, drag and drop, or right-click in the empty area below the tracks

### Patterns and song
- Up to 16 patterns (tabs above the tracks, with name and colour; double-click a tab to rename it). Each pattern has its own tracks, with their volume, pan, mute, solo, Echo and Reverb, its own audio clips and loop length
- Drum and bass patterns are a shared library (D01..D16, B01..B16). A seven-segment counter in each card chooses which one the open pattern plays; counting to a new number starts an empty pattern. Several patterns can play the same one
- Song mode: one row of pattern blocks in play order, with repeats (drag to reorder, wheel to change repeats, right-click for the menu). Play renders the whole song and plays it; export bounces the whole song

### Mixer
- Level meters, volume knob, pan, mute, solo, plus Echo and Reverb on every track row
- Master strip with stereo meter, next to the tuner
### Tuner
- Chromatic tuner next to the master strip, fed from the input of the armed track (or track 1). Scrolling cent ruler, note name, Hz and cents, adjustable A reference (430-450 Hz, remembered)

### Export and projects
- A project is a folder with a `<name>.riffcatch` project file (JSON) and one WAV per track. Save as asks for a name and creates the folder; double-click a `.riffcatch` file (registered for your user automatically, or via File > Open .riffcatch files with RiffCatch) or drop it on the window to open it. Only one RiffCatch window runs at a time
- File > Open recent lists the last ten projects
- A configurable work folder (File > Work folder...) is where Save as, Open and Export start. The folder button in the top bar opens the current project folder in Explorer (the work folder while untitled)
- Project files are read defensively: track files must sit in the project folder, sizes and values are limited, and saving refuses folders that hold other files
- Export mix to WAV or MP3 (LAME)
- Free-text project notes (Notes button in the top bar), saved with the project and the autosave

### Themes
- Nine built-in themes, switched from the File menu or with `Ctrl+Shift+O` / `Ctrl+Shift+P` (previous / next)
- Custom theme generator (File > Theme > Custom theme...): pick a primary and an accent colour plus dark or light, and the rest of the palette is derived with a live preview. Saved themes are stored as JSON in `%APPDATA%\RiffCatch\themes` and appear in the Theme menu

![RiffCatch main window, Midnight theme](docs/screenshots/main-midnight.png)

## Building

Requirements: Windows, Visual Studio 2022 (MSVC), CMake 3.22 or newer. JUCE 8.0.4 is fetched automatically by CMake.

```
cmake -S . -B build
cmake --build build --config Release --parallel
```

The executable ends up in `build/RiffCatch_artefacts/Release/RiffCatch.exe`.

### ASIO

The ASIO SDK cannot be redistributed. Download it from Steinberg and extract it to `third_party/asio_sdk` (so that `third_party/asio_sdk/common/iasiodrv.h` exists). Without it the build uses WASAPI only.

### MP3 export

MP3 export uses LAME (LGPL). Extract the LAME 3.100 source to `third_party/lame`. Without it the build has no MP3 export.

### Self test

```
RiffCatch.exe --selftest
```

Writes `selftest.txt` next to the executable and ends with `ALL OK` when everything passes. Close any running RiffCatch first: only one instance runs at a time, so a second start is handed to the running one.

## License

TBD.
