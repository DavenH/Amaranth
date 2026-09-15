# Piano pitch-labeling utility

This disposable browser tool manually creates and corrects onset/MIDI labels
for `oscillo/content/piano-notes-all.mp3`.

From the repository root, serve the Oscillo directory:

```sh
python3 -m http.server 8765 --directory oscillo
```

Then open <http://localhost:8765/tools/pitch-labeler.html> and choose **Load
repository recording**. Use the up/down arrow keys to select the current MIDI
note. Left/right moves the audio cursor by 500 ms, or 2 seconds while holding Shift.
Clicking empty waveform space adds a strike for the current note; clicking or
dragging near an existing marker selects or repositions it.

Annotate the five strikes for each key, advance to the next note, and export
JSON. Existing JSON can be imported to resume a session.
The output uses the same flat label schema as `content/tuning-sample.json`:

```json
{"time_sec": 0.664, "midi": 21, "note": "A0", "freq_hz": 27.5, "confidence": 0.91}
```
