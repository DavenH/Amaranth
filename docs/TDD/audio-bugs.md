# Audio Bug Notes

## Open: Native Envelope edit smoke exports non-finite audio samples

Context:

- The default-graph Envelope native sequence completes its editor gesture and
  causal-update assertions, then `captureAudio` serializes at least one sample
  as JSON `null`, indicating a non-finite value.
- `assert_audio_changed` consequently fails while comparing the initial and
  post-edit 2048-frame captures. The issue reproduces when the Envelope
  sequence runs alone and is independent of the Stengah Pan/cursor work.

Current status: open; locate the first non-finite producer in the default graph
after the live Envelope mesh edit and retain the audio assertion as a failing
gate rather than accepting or filtering the samples.
