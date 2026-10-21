# Saxophone reference acquisition

Acquired 2026-10-21 after the user authorized web reference downloads. Original
media and acquisition records live under `.context/sax-reference-downloads/`
at the workspace root. They are separate from the completed, frozen one-growl
pilot; that experiment and its results have not been changed to imply broader
validation. No downloaded media is included in Git or the product's factory
bank. Use the existing reference library/study owners for subsequent imports.

## Iowa alto: register, dynamics and vibrato

[Source page](https://theremin.music.uiowa.edu/MISaltosaxophone.html) and
[published usage statement](https://theremin.music.uiowa.edu/MIS.html).
The publisher credits Michael Giles, November 1997, anechoic recording with a
Neumann KM 84 five feet away. The collection description states recording input
gain stayed constant across dynamics. Its published use statement permits
download and use for projects without restrictions; saved source snapshots
retain the wording and provenance.

Downloaded both original archives:

| Archive | SHA256 |
| --- | --- |
| [AltoSax.NoVib.zip](https://theremin.music.uiowa.edu/sound%20files/MIS/Woodwinds/altosaxophone/AltoSax.NoVib.zip) | `670ac70e0a85031f46f9b5a39fb76731487a444b5fc19ceb979437345e017a7b` |
| [AltoSax.Vib.zip](https://theremin.music.uiowa.edu/sound%20files/MIS/Woodwinds/altosaxophone/AltoSax.Vib.zip) | `b00cf1f44d3493bd72b7f8acd2f3e32ffa7dfb7b549a8b4f7027344f067d94f6` |

Safe extraction produced 18 original chromatic-scale AIFFs: mono 44.1 kHz,
16-bit PCM, 1,066.914 seconds total. Published labels span `pp/mf/ff`,
`NoVib/Vib`, and `Db3B3/C4B4/C5Ab5` register bands. These are scale recordings,
not isolated-note files; there are no per-note markers. Preserve pitch tokens
verbatim until sounding frequency/transposition is verified. Preserve original
PCM levels before the app's peak-normalizing reference analysis.

Partition was recorded before acquisition: both `pp.C5Ab5` technique files are
reserved as one paired parent group; the other 16 files are objective. Future
regions inherit their parent partition. This tests a register/dynamic
combination within one performer/session, not independent-session fidelity.
Do not count crops or later repackaging of the 1997 recordings as new takes.

Local records: `iowa/collection.json`, `partition.json`,
`collection-freeze.json` and `README.md`. All 28 frozen acquisition artifacts
were verified. No crops, conversion, audition or app import occurred during
acquisition. Controlled growl, breath/buzz, continuous dynamics and independent
retakes remain missing.

## VCSL tenor: isolated notes and articulation contrasts

[Upstream VCSL](https://github.com/sgossner/VCSL), pinned at
`c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e`, supplies original
[CC0 recordings](https://github.com/sgossner/VCSL/blob/c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e/LICENSE).
Downloaded 12 raw WAVs (20,453,946 bytes): 48 kHz, stereo, 24-bit PCM. None
contains sampler-loop metadata. Prefer these originals over the
[FreePats derivative](https://freepats.zenvoid.org/Reed/saxophone.html), which
adds editing and infinite sustain loops to the same recording material.

The selection was frozen before retrieval/analysis. Eight objective files use
non-vibrato C2/C4 at layers 2/3, staccato C3 at layers 1/2, and vibrato C2/C4.
Four reserved files use D3 across those articulations/layers. The upstream
filenames contain the performer token `Brett`; performer identity, sounding
octave convention and mapping of velocity layers to dynamic markings remain
unverified. No independent-player holdout is claimed.

Local `vcsl/download-manifest.json` records each pinned URL, local path,
SHA256, Git blob SHA1, original labels and audio header. Its SHA256 is
`426faf7b9dc662b252c7ebbbffe1ec8c4d5a08a7ab22b94c3b3b73736ebe6d39`.
The predeclared split is `vcsl/selection.json`; original licence/source snapshots
are retained beside it. Reserved audio has not been used to fit or select a
candidate.

The byte-identical acquisition manifest, selection and licence are now tracked
in [vcsl-corpus](vcsl-corpus/README.md), with a reconstruction recipe. The
calibration's strict metadata identities therefore do not depend on access to
the original workspace's ignored files. Downloaded audio remains local.

## Continuous solo supplied by the user

[Ivan Renta, tenor sax solo](https://www.youtube.com/watch?v=89A0vvoeI6M),
uploaded by `congahead`, has a reported duration of 594 seconds. The original
public audio stream was downloaded locally as WebM/Opus, with metadata and
SHA256 in `youtube/acquisition.json`. It is a lossy distribution stream, not an
unprocessed recording master. Redistribution permission is unspecified; keep
the media local rather than publishing it in the repository or reports.

The media remains uncropped and unanalysed. Preserve complete phrases as well
as proposed steady-note windows. Annotate source offsets, attacks, slurs, bends,
vibrato and uncertain regions; do not infer MIDI control settings or observed
dynamic/pitch labels from the title. Reserve whole phrases before refinement;
nearby frames from the same phrase are not independent held-out examples.

## Commercial catalogue leads

The [Noiiz saxophone catalogue](https://www.noiiz.com/sounds/instruments/136)
exposes catalogue information, but downloads require an account. No account,
purchase or media download was made there.

The [Splice saxophone catalogue](https://splice.com/sounds/instruments/brass-woodwinds-saxophone/samples)
provides public catalogue/preview information. Its
[terms](https://splice.com/terms), section 3.1.1.3(f), limit preview use to the
service; the [licensing FAQ](https://support.splice.com/en/articles/8652642-splice-sounds-licensing-faq)
also describes AI-related restrictions. Public preview access was not treated
as an offline modelling licence. No preview assets were extracted. The local
research record is `splice/research.json`; use Iowa/VCSL for the current offline
reference collection.
