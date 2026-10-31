# AI creative mode for RiffCatch

Principle: the LLM writes *data* (patterns, key, tempo, synth parameters), never audio. The app validates and clamps everything and applies it through the existing undo system. Audio is analysed locally (DSP) and only a text summary goes to the model. Provider: any OpenAI-compatible endpoint (Ollama by default, `http://localhost:11434/v1`; also LM Studio, llama.cpp server, or a cloud service). No model is shipped; the feature degrades gracefully when no endpoint answers (like ASIO/LAME).

Machine: RTX 4070 12 GB, Ollama not installed yet -> suggest a 7-14B instruct model (e.g. Qwen2.5 or Llama 3.1 class); README gets a setup note.

## M1 (v0.50): chat panel + prompt to groove
- `Source/ai/LlmClient.h/.cpp`: juce::Thread worker, POST `/v1/chat/completions` with a JSON schema response format (fallback: extract the first JSON object from free text), `/v1/models` for "Test connection", timeout, cancel, error messages. Settings in the user settings file: endpoint, model, API key (optional), temperature.
- `Source/ai/ProjectSummary.h`: compact text of the current state: tempo, time signature, loop bars, key/scale, drum strings per voice per bar (`x-X-...`), bass notes (`bar.step degree len flags`), chord shifts, synth params, track list. Replaces earlier state in each turn so the context stays small.
- `Source/ai/Proposal.h`: schema + parser + clamp/repair + apply. Fields: explanation, tempo, timeSignature, loopBars, key {root, scale}, drums {bars, voices as step strings}, bass {bars, notes[], progression[], sound or params}, later lead / pad / arrangement. Model speaks scale degrees and roman numerals, drums as strings, or picks a template id ("lock to kick"); the app converts. Applied as ONE undo step (`MainComponent::record`).
- `Source/ui/AiPanel.h/.cpp`: docked chat drawer (toggle button next to Notes, shortcut chosen to avoid existing Ctrl+ combos). Message list (user / model explanation), input, Send / Stop, Regenerate, lock toggles (Drums, Bass, Sound, Tempo+key) restricting what the model may change, settings popover. Runs async with a progress indicator.
- Selftest: parser repair (broken JSON, out-of-range values, unknown fields), apply + undo, summary stability, fake local HTTP server for LlmClient (success, error, timeout).

## M2 (v0.51): audio analysis (wav / recorded tracks)
- `Source/audio/Analysis.h`: onset detection (spectral flux), tempo + beat phase (onset autocorrelation, octave resolved against a plausible range), key (chroma + Krumhansl-Schmuckler, mapped to our scales), monophonic riff transcription (existing YIN + onset segmentation, quantised to the 16th grid) -> note list.
- UI: track menu "Analyze..." -> result card with buttons: Set project tempo, Set key, Transcribe to bass (useful without any LLM), Send to AI.
- The prompt automatically includes the analysis of the track you mention or have selected ("write a bass line and drums for track 2").
- Selftest: synthetic clicks -> BPM, synthetic scale -> key, synthetic melody -> notes. Polyphonic transcription (ONNX Basic Pitch) stays optional/later.

## M3 (v0.52): sound from words, variants, locks
- "warm, dubby, wobbly" -> synth params and echo/reverb (parameter ranges are listed in the system prompt, values clamped).
- Variants: ask for 3-4 proposals, audition each (applied temporarily, committed only when you choose), regenerate one part while the others stay locked.

## M4 (v0.53+): more instruments and sections (needed for a full song)
- Generalise the bass voice: Lead (mono, shares the piano-roll panel) first, then Pad / chords (poly, slow attack, roman-numeral chord row). New tabs: Drums | Bass | Lead | Chords.
- Sections: pattern banks A/B/C per instrument and an arrangement row (A A B A) choosing the bank per loop pass; the AI can write the arrangement.

## Risks
- Small local models are weak at rhythm and theory -> constrained vocabulary (degrees, strings, templates), validation, repair, temperature and seed controls.
- Latency 5-30 s -> background thread, progress, cancel.
- Privacy: nothing but text summaries leaves the machine; fully offline with Ollama.
