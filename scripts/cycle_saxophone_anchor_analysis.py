#!/usr/bin/env python3
"""Measure developmental saxophone anchors using existing exported pitch paths.

This is offline authoring evidence, not a pitch detector or sample instrument.
Requires NumPy; plotting additionally requires Matplotlib.
"""

import argparse
import json
import math
from pathlib import Path

import numpy as np

from cycle_audio_diff import midi_frequency, read_wav
from cycle_reference_pitch_evidence import pitch_evidence
from cycle_reference_study import file_digest


ROOT = Path(__file__).resolve().parents[1]
ANCHORS = {
    "BrettTenor_NV_Main_D3_vl2_rr1": {"layer": 2, "bodyEnd": 6.65, "tailStart": 6.75},
    "BrettTenor_NV_Main_D3_vl3_rr1": {"layer": 3, "bodyEnd": 5.65, "tailStart": 5.82},
}
FFT_SIZE, HOP_SECONDS, HARMONICS = 4096, .01, 64


def db(value):
    return float(20 * np.log10(max(float(value), 1e-12)))


def energy_envelope(channels, rate):
    """Channel-energy RMS without phase-cancelling stereo downmix or normalization."""
    size = round(rate * HOP_SECONDS)
    count = channels.shape[1] // size
    blocks = channels[:, :count * size].reshape(channels.shape[0], count, size)
    rms = np.sqrt(np.mean(blocks ** 2, axis=(0, 2)))
    times = (np.arange(count) + .5) * size / rate
    return times, rms


def spectral_trace(channels, rate, pitch_frames, harmonic_count=HARMONICS):
    """Hann spectral-band energy around supplied f0; does not estimate pitch.

    The complement of harmonic bands includes noise, room/release sound,
    unresolved modulation and FFT leakage. It is not a separated noise stem.
    """
    hop = round(rate * HOP_SECONDS)
    padded = np.pad(channels, ((0, 0), (FFT_SIZE // 2, FFT_SIZE // 2)))
    frames = np.lib.stride_tricks.sliding_window_view(padded, FFT_SIZE, axis=1)[:, ::hop]
    window = np.hanning(FFT_SIZE)
    transformed = np.fft.rfft(frames * window, axis=2)
    channel_power = abs(transformed) ** 2 * 2 / (FFT_SIZE * np.sum(window ** 2))
    channel_power[:, :, [0, -1]] *= .5
    power = np.mean(channel_power, axis=0)
    times = np.arange(len(power)) * hop / rate
    pitch_times = np.array([frame["timeSeconds"] for frame in pitch_frames])
    pitch_frequencies = np.array([frame["frequencyHz"] for frame in pitch_frames])
    f0 = np.interp(times, pitch_times, pitch_frequencies)
    frequencies = np.fft.rfftfreq(FFT_SIZE, 1 / rate)
    bandwidth = 2 * rate / FFT_SIZE
    channel_harmonic_power = []
    covered = np.zeros(power.shape, dtype=bool)
    for harmonic in range(1, harmonic_count + 1):
        mask = abs(frequencies[None, :] - f0[:, None] * harmonic) <= bandwidth
        channel_harmonic_power.append(np.sum(np.where(mask[None, :, :], channel_power, 0), axis=2))
        covered |= mask
    channel_harmonic_power = np.array(channel_harmonic_power).transpose(1, 2, 0)
    harmonic_power = np.mean(channel_harmonic_power, axis=0)
    total = np.sum(power, axis=1)
    return {"times": times, "f0": f0, "power": power, "frequencies": frequencies,
            "harmonicPower": harmonic_power, "totalPower": total,
            "channelHarmonicPower": channel_harmonic_power,
            "remainingPower": np.sum(np.where(covered, 0, power), axis=1),
            "centroid": np.sum(power * frequencies, axis=1) / np.maximum(total, 1e-24)}


def summarize_interval(trace, start, end):
    selected = (trace["times"] >= start) & (trace["times"] < end)
    if np.count_nonzero(selected) < 2:
        raise ValueError("Spectral interval requires at least two measured frames")
    powers = np.mean(trace["harmonicPower"][selected], axis=0)
    total = float(np.mean(trace["totalPower"][selected]))
    amplitudes = np.sqrt(2 * powers)
    channel_amplitudes = np.sqrt(2 * np.mean(trace["channelHarmonicPower"][:, selected], axis=1))
    return {"startSeconds": start, "endSeconds": end,
            "frameCount": int(np.count_nonzero(selected)),
            "meanF0Hz": float(np.mean(trace["f0"][selected])),
            "centroidHz": float(np.mean(trace["centroid"][selected])),
            "spectralRms": math.sqrt(total),
            "remainingBandFraction": float(np.mean(trace["remainingPower"][selected])) / max(total, 1e-24),
            "harmonicPeakAmplitudes": amplitudes.tolist(),
            "channelHarmonicPeakAmplitudes": channel_amplitudes.tolist(),
            "channelHarmonicPeakAmplitudesToStereoRms": (channel_amplitudes / max(math.sqrt(total), 1e-12)).tolist(),
            "harmonicRatiosToFundamental": (amplitudes / max(amplitudes[0], 1e-12)).tolist(),
            "harmonicDbToFundamental": [db(value / max(amplitudes[0], 1e-12)) for value in amplitudes],
            "harmonicPowerFractions": (powers / max(total, 1e-24)).tolist()}


def harmonic_phase(channel, rate, pitch_frames, start, end, harmonic_count=48):
    """Fit cosine phases to an integrated canonical pitch path, without tracking.

    Positive coefficient phase means cos(h * theta + phase). Removing h times
    the fundamental phase makes the result invariant to the cycle origin.
    """
    first, stop = round(start * rate), round(end * rate)
    signal = channel[first:stop]
    if len(signal) != stop - first or len(signal) < harmonic_count * 4:
        raise ValueError("Phase interval lies outside the audio or is too short")
    times = np.arange(first, stop) / rate
    frequencies = np.interp(times, [frame["timeSeconds"] for frame in pitch_frames],
                            [frame["frequencyHz"] for frame in pitch_frames])
    theta = np.concatenate(([0], np.cumsum((frequencies[:-1] + frequencies[1:]) * np.pi / rate)))
    harmonics = np.arange(1, harmonic_count + 1)
    angles = theta[:, None] * harmonics
    design = np.column_stack((np.cos(angles), np.sin(angles)))
    weights = np.sqrt(np.hanning(len(signal)))
    coefficients = np.linalg.lstsq(design * weights[:, None], signal * weights, rcond=None)[0]
    complex_coefficients = coefficients[:harmonic_count] - 1j * coefficients[harmonic_count:]
    amplitudes = abs(complex_coefficients)
    relative = np.angle(np.exp(1j * (np.angle(complex_coefficients)
                                    - harmonics * np.angle(complex_coefficients[0]))))
    residual = (signal - design @ coefficients) * weights
    return {"startSeconds": start, "endSeconds": end,
            "fundamentalPhaseRadians": float(np.angle(complex_coefficients[0])),
            "relativePhaseRadians": relative.tolist(),
            "peakAmplitudes": amplitudes.tolist(),
            "dbToFundamental": [db(value / max(amplitudes[0], 1e-12)) for value in amplitudes],
            "weightedResidualRms": float(np.sqrt(np.sum(residual ** 2) / np.sum(weights ** 2)))}


def phase_targets(channels, rate, pitch_frames, body_end):
    settled = harmonic_phase(channels[0], rate, pitch_frames, 1.5, 2)
    windows = [harmonic_phase(channels[0], rate, pitch_frames, start, end) for start, end in
               ((.4, .6), (.6, .8), (1.5, 1.7), (1.7, 1.9), (3, 3.2), (body_end - .3, body_end))]
    phases = np.array([window["relativePhaseRadians"] for window in windows])
    differences = np.angle(np.exp(1j * (phases - settled["relativePhaseRadians"])))
    return {"channel": 0, "convention": "cos(h*theta+phi_h); relative phase phi_h-h*phi_1, wrapped -pi..pi",
            "method": "Hann-weighted joint least squares of 48 sin/cos pairs against integrated interpolated canonical f0",
            "settled": settled, "bodyWindows": windows,
            "channels": [{"channel": index, "settled": settled if index == 0 else
                          harmonic_phase(channel, rate, pitch_frames, 1.5, 2)}
                         for index, channel in enumerate(channels)],
            "maxBodyDeviationDegrees": np.degrees(np.max(abs(differences), axis=0)).tolist(),
            "rmsBodyDeviationDegrees": np.degrees(np.sqrt(np.mean(differences ** 2, axis=0))).tolist()}


def envelope_error(source_times, source_rms, render_times, render_rms, start, end, gain):
    selected = ((source_times >= start) & (source_times < end)
                & (source_times <= render_times[-1]))
    target = source_rms[selected]
    actual = np.interp(source_times[selected], render_times, render_rms)
    audible = target >= np.max(source_rms) * .01
    def errors(values):
        return {"meanAbsoluteRmsError": float(np.mean(abs(values - target))),
                "rmseToSourcePeakRms": float(np.sqrt(np.mean((values - target) ** 2)) / np.max(source_rms)),
                "audibleMeanAbsoluteDbError": float(np.mean(abs(20 * np.log10(np.maximum(values[audible], 1e-12)
                                                                     / np.maximum(target[audible], 1e-12))))) if np.any(audible) else None}
    return {"frameCount": len(target), "raw": errors(actual), "globalGainMatched": errors(actual * gain)}


def compare_render(anchor, render_path):
    """Compare an elapsed-time-aligned render with its source; no pitch detection.

    Uses one gain over the complete shared recording interval. Phase-local
    harmonic shapes are reported separately; their level is never normalized.
    Missing source intervals stay unavailable rather than silently disappearing.
    """
    source_path, analysis_path, render_path = (Path(anchor["sourcePath"]),
                                              Path(anchor["pitchAnalysisPath"]), Path(render_path))
    if (file_digest(source_path) != anchor["sourceSha256"]
            or file_digest(analysis_path) != anchor["pitchAnalysisSha256"]):
        raise ValueError("Pinned source or pitch evidence changed")
    exported = json.loads(analysis_path.read_text())
    if exported["lowConfidence"] or exported["trackingUsedFallback"]:
        raise ValueError("Comparison requires confident canonical pitch evidence")
    source, render = read_wav(source_path), read_wav(render_path)
    source_channels, render_channels = np.array(source["channels"]), np.array(render["channels"])
    source_times, source_rms = energy_envelope(source_channels, source["sampleRate"])
    render_times, render_rms = energy_envelope(render_channels, render["sampleRate"])
    duration = render["frames"] / render["sampleRate"]
    shared_end = min(anchor["durationSeconds"], duration)
    if shared_end < .1:
        raise ValueError("Comparison requires at least 100ms shared audio")
    raw_source_rms = float(np.sqrt(np.mean(source_channels[:, :round(shared_end * source["sampleRate"])] ** 2)))
    raw_render_rms = float(np.sqrt(np.mean(render_channels[:, :round(shared_end * render["sampleRate"])] ** 2)))
    if raw_render_rms < 1e-10:
        raise ValueError("Render is silent; gain matching and timing comparison are undefined")
    gain = raw_source_rms / max(raw_render_rms, 1e-12)
    trace = spectral_trace(render_channels, render["sampleRate"], exported["pitchFrames"])
    intervals = {}
    for name, target in anchor["spectralTargets"].items():
        start, end = target["startSeconds"], target["endSeconds"]
        if duration < end:
            intervals[name] = {"available": False, "reason": "Render does not cover complete source interval",
                               "startSeconds": start, "endSeconds": end}
            continue
        actual = summarize_interval(trace, start, end)
        target_amplitudes = np.array(target["harmonicPeakAmplitudes"][:32])
        actual_amplitudes = np.array(actual["harmonicPeakAmplitudes"][:32])
        audible = target_amplitudes >= target_amplitudes[0] * 10 ** (-45 / 20)
        matched = np.maximum(actual_amplitudes[audible] * gain, 1e-12)
        log_error = 20 * np.log10(matched / np.maximum(target_amplitudes[audible], 1e-12))
        target_ratios = target_amplitudes / max(target_amplitudes[0], 1e-12)
        actual_ratios = actual_amplitudes / max(actual_amplitudes[0], 1e-12)
        intervals[name] = {"available": True, "startSeconds": start, "endSeconds": end,
                           "audibleHarmonics": (np.flatnonzero(audible) + 1).tolist(),
                           "globalGainMatchedHarmonicMeanAbsoluteDbError": float(np.mean(abs(log_error))),
                           "globalGainMatchedHarmonicDbRmse": float(np.sqrt(np.mean(log_error ** 2))),
                           "relativeHarmonicShapeRmse": float(np.sqrt(np.mean((actual_ratios[audible] - target_ratios[audible]) ** 2))),
                           "renderHarmonicPeakAmplitudes": actual["harmonicPeakAmplitudes"],
                           "centroidHz": actual["centroidHz"], "sourceCentroidHz": target["centroidHz"],
                           "remainingBandFraction": actual["remainingBandFraction"],
                           "sourceRemainingBandFraction": target["remainingBandFraction"],
                           "envelope": envelope_error(source_times, source_rms, render_times, render_rms, start, end, gain)}
    timings = {}
    for name, fraction in (("attack10Percent", .1), ("attack50Percent", .5), ("attack90Percent", .9)):
        crossing = float(render_times[np.flatnonzero(render_rms >= np.max(render_rms) * fraction)[0]])
        timings[name + "Seconds"] = crossing
        timings[name + "ErrorSeconds"] = crossing - anchor["timings"][name + "Seconds"]
    body = (render_times >= .5) & (render_times < anchor["timings"]["selectedBodyEndSeconds"])
    body_rms = float(np.median(render_rms[body])) if np.any(body) else None
    for attenuation in (6, 20, 40):
        name = f"tailMinus{attenuation}DbSeconds"
        indices = np.flatnonzero((render_times >= anchor["timings"]["selectedTailStartSeconds"])
                                 & (render_rms <= (body_rms or 0) * 10 ** (-attenuation / 20)))
        crossing = float(render_times[indices[0]]) if len(indices) else None
        timings[name] = crossing
        timings[name + "ErrorSeconds"] = (crossing - anchor["timings"][name]
                                          if crossing is not None and anchor["timings"][name] is not None else None)
    return {"schema": "cycle-saxophone-anchor-render-comparison.v1", "anchorId": anchor["id"],
            "renderPath": str(render_path.resolve()), "renderSha256": file_digest(render_path),
            "sourceSha256": anchor["sourceSha256"], "renderDurationSeconds": duration,
            "sourceDurationSeconds": anchor["durationSeconds"], "sharedDurationSeconds": shared_end,
            "policy": "No time alignment, local gain fitting, pitch tracking or waveform correlation; one global channel-energy RMS gain over complete shared time",
            "harmonicPolicy": "H1..32 with source phase-mean band amplitude >= -45dB relative to source fundamental",
            "rawSourceRms": raw_source_rms, "rawRenderRms": raw_render_rms,
            "renderRmsErrorDb": db(raw_render_rms / max(raw_source_rms, 1e-12)),
            "globalGain": gain, "globalGainDb": db(gain), "timings": timings,
            "envelope": envelope_error(source_times, source_rms, render_times, render_rms, 0, shared_end, gain),
            "intervals": intervals}


def bridge_measurement(wav_path, expected_standard_midi=62, start=.4, end=2.0):
    """Describe a held bridge interval using expected, untracked pitch."""
    decoded = read_wav(wav_path)
    channels, rate = np.array(decoded["channels"]), decoded["sampleRate"]
    if not 0 <= start < end <= decoded["frames"] / rate:
        raise ValueError("Bridge measurement interval lies outside the recording")
    frequency = midi_frequency(expected_standard_midi)
    path = [{"timeSeconds": 0, "frequencyHz": frequency},
            {"timeSeconds": decoded["frames"] / rate, "frequencyHz": frequency}]
    measured = summarize_interval(spectral_trace(channels, rate, path), start, end)
    selected = channels[:, round(start * rate):round(end * rate)]
    times, rms = energy_envelope(selected, rate)
    amplitudes = np.array(measured["harmonicPeakAmplitudes"])
    derivative = np.diff(selected, axis=1)
    return {"pitchPolicy": "Expected frequency, not tracked pitch", "expectedStandardMidi": expected_standard_midi,
            "expectedFrequencyHz": frequency, "startSeconds": start, "endSeconds": end,
            "channelEnergyRms": float(np.sqrt(np.mean(selected ** 2))), "peak": float(np.max(abs(selected))),
            "centroidHz": measured["centroidHz"], "remainingBandFraction": measured["remainingBandFraction"],
            "normalizedHarmonicShape": (amplitudes / max(float(np.linalg.norm(amplitudes)), 1e-12)).tolist(),
            "maximumSampleJump": float(np.max(abs(derivative))), "derivativeRms": float(np.sqrt(np.mean(derivative ** 2))),
            "silent10msFrames": int(np.count_nonzero(rms < max(float(np.max(rms)) * .001, 1e-10))),
            "envelope10ms": [{"timeSeconds": float(time + start), "rms": float(value)} for time, value in zip(times, rms)]}


def plot_render_comparison(anchor, render_path, output_path):
    """Save original/matched envelope and phase-specific harmonic comparisons."""
    import matplotlib
    matplotlib.use("Agg")
    from matplotlib import pyplot as plt
    comparison = compare_render(anchor, render_path)
    figure, axes = plt.subplots(4, 1, figsize=(11, 11))
    for path, label, color in ((anchor["sourcePath"], "Source", "#303030"),
                               (render_path, "Synth, original level", "#2869a6")):
        decoded = read_wav(path)
        times, rms = energy_envelope(np.array(decoded["channels"]), decoded["sampleRate"])
        axes[0].plot(times, 20 * np.log10(np.maximum(rms, 1e-12)), label=label, color=color)
        if label.startswith("Synth"):
            axes[0].plot(times, 20 * np.log10(np.maximum(rms * comparison["globalGain"], 1e-12)),
                         label="Synth, one global RMS gain", color="#ce662d", alpha=.8)
    axes[0].set_ylabel("10ms channel-energy RMS (dBFS)")
    axes[0].set_xlabel("Elapsed time (seconds); no alignment")
    axes[0].set_ylim(-85, -10)
    for axis, name in zip(axes[1:], ("earlyBody", "settledBody", "tail")):
        interval = comparison["intervals"].get(name)
        if not interval or not interval["available"]:
            axis.set_title(name + ": source interval not covered by render")
            continue
        target = np.array(anchor["spectralTargets"][name]["harmonicPeakAmplitudes"][:32])
        actual = np.array(interval["renderHarmonicPeakAmplitudes"][:32]) * comparison["globalGain"]
        harmonics = np.arange(1, len(target) + 1)
        axis.plot(harmonics, 20 * np.log10(np.maximum(target, 1e-12)), label="Source", color="#303030")
        axis.plot(harmonics, 20 * np.log10(np.maximum(actual, 1e-12)), label="Synth, one global RMS gain", color="#ce662d")
        axis.set_title(f"{name}: mean harmonic error {interval['globalGainMatchedHarmonicMeanAbsoluteDbError']:.2f}dB "
                       "for audible H1–32")
        axis.set_ylabel("Harmonic peak amplitude (dBFS)")
        axis.set_xlabel("Harmonic number")
    for axis in axes:
        axis.grid(alpha=.2)
        axis.legend(fontsize=8)
    figure.suptitle(anchor["id"] + " — developmental source / synthesis comparison")
    figure.tight_layout()
    figure.savefig(output_path, dpi=130)
    plt.close(figure)
    return comparison


def analyze_anchor(source, exported, exported_path, source_sha256, design):
    if file_digest(source) != source_sha256:
        raise ValueError("Reference audio no longer matches its pinned analysis")
    decoded = read_wav(source)
    channels = np.array(decoded["channels"])
    rate = decoded["sampleRate"]
    duration = decoded["frames"] / rate
    if abs(duration - exported["durationSeconds"]) > 1 / rate:
        raise ValueError("Reference duration does not match saved pitch export")
    evidence = pitch_evidence(exported, .3, design["bodyEnd"], 62)
    if not evidence["available"]:
        raise ValueError("Trustworthy canonical pitch-path evidence is required")
    trace = spectral_trace(channels, rate, exported["pitchFrames"])
    times, rms = energy_envelope(channels, rate)
    peak_rms = float(np.max(rms))
    body = (times >= .5) & (times < design["bodyEnd"])
    body_rms = float(np.median(rms[body]))
    crossings = {}
    for name, fraction in (("attack10Percent", .1), ("attack50Percent", .5), ("attack90Percent", .9)):
        crossings[name + "Seconds"] = float(times[np.flatnonzero(rms >= peak_rms * fraction)[0]])
    for attenuation in (6, 20, 40):
        indices = np.flatnonzero((times >= design["tailStart"]) & (rms <= body_rms * 10 ** (-attenuation / 20)))
        crossings[f"tailMinus{attenuation}DbSeconds"] = float(times[indices[0]]) if len(indices) else None
    control_times = sorted(set([0, .01, .02, .03, .04, .05, .075, .1, .15, .2, .3, .5,
                               1, 1.5, 2, 3, 4, 5, design["bodyEnd"], design["tailStart"]]
                              + [design["tailStart"] + offset for offset in (0, .05, .1, .15, .2, .3, .4, .5, .6)]))
    controls = [{"timeSeconds": time, "rms": float(np.interp(time, times, rms)),
                 "gainToPeakRms": float(np.interp(time, times, rms)) / peak_rms,
                 "dbToPeakRms": db(float(np.interp(time, times, rms)) / peak_rms)}
                for time in control_times if time < duration]
    stages = {"attack": (.02, .08), "opening": (.08, .2), "earlyBody": (.3, .8),
              "settledBody": (1.5, 2.5), "middleBody": (3, 4),
              "lateBody": (design["bodyEnd"] - .5, design["bodyEnd"]),
              "tail": (design["tailStart"], min(duration, design["tailStart"] + .35))}
    result = {"id": source.stem, "partition": "developmental", "sourcePath": str(source),
              "sourceSha256": source_sha256, "pitchAnalysisPath": str(exported_path),
              "pitchAnalysisSha256": file_digest(exported_path), "sampleRate": rate,
              "channels": len(channels), "durationSeconds": duration,
              "sourceLayer": design["layer"], "standardMidiInferred": 62,
              "pitchEvidence": {key: value for key, value in evidence.items() if key != "frames"},
              "rawLevels": {"peak": float(np.max(abs(channels))), "fullRms": float(np.sqrt(np.mean(channels ** 2))),
                            "channelFullRms": np.sqrt(np.mean(channels ** 2, axis=1)).tolist(),
                            "bodyMedian10msRms": body_rms, "peak10msRms": peak_rms},
              "timings": {**crossings, "selectedBodyEndSeconds": design["bodyEnd"],
                          "selectedTailStartSeconds": design["tailStart"],
                          "noteOffSeconds": None, "noteOffPolicy": "Not known from audio; tail start is an author-selected region"},
              "volumeControlTargets": controls,
              "phaseTargets": phase_targets(channels, rate, exported["pitchFrames"], design["bodyEnd"]),
              "spectralTargets": {name: summarize_interval(trace, *interval) for name, interval in stages.items()},
              "trace": [{"timeSeconds": float(time), "f0Hz": float(trace["f0"][index]),
                         "spectralRms": float(np.sqrt(trace["totalPower"][index])),
                         "centroidHz": float(trace["centroid"][index]),
                         "remainingBandFraction": float(trace["remainingPower"][index] / max(trace["totalPower"][index], 1e-24)),
                         "harmonicPeakAmplitudes": np.sqrt(2 * trace["harmonicPower"][index]).tolist(),
                         "channelHarmonicPeakAmplitudes": np.sqrt(2 * trace["channelHarmonicPower"][:, index]).tolist()}
                        for index, time in enumerate(trace["times"])]}
    return result, (times, rms, trace)


def plot(result, data, output):
    import matplotlib
    matplotlib.use("Agg")
    from matplotlib import pyplot as plt
    figure, axes = plt.subplots(3, 1, figsize=(12, 9), sharex=True)
    for anchor, (times, rms, trace), color in zip(result["anchors"], data, ("#2869a6", "#ce662d")):
        label = "vl" + str(anchor["sourceLayer"])
        axes[0].plot(times, 20 * np.log10(np.maximum(rms, 1e-12)), color=color, label=label)
        axes[1].plot(trace["times"], trace["centroid"], color=color, label=label)
        axes[2].plot(trace["times"], trace["remainingPower"] / np.maximum(trace["totalPower"], 1e-24), color=color, label=label)
    for axis, ylabel in zip(axes, ("10 ms RMS (dBFS)", "Power centroid (Hz)", "Remaining-band power fraction")):
        axis.set_ylabel(ylabel)
        axis.grid(alpha=.2)
        axis.legend()
    axes[0].set_ylim(-85, -18)
    axes[-1].set_xlabel("Original elapsed recording time (seconds); no alignment or normalization")
    figure.suptitle("Developmental D3 tenor anchors — original levels and full note evolution")
    figure.tight_layout()
    figure.savefig(output / "anchor-evolution.png", dpi=130)
    plt.close(figure)
    figure, axes = plt.subplots(1, 2, figsize=(13, 4), sharey=True)
    for axis, anchor in zip(axes, result["anchors"]):
        for stage in ("opening", "earlyBody", "settledBody", "lateBody"):
            axis.plot(np.arange(1, HARMONICS + 1), anchor["spectralTargets"][stage]["harmonicDbToFundamental"], label=stage)
        axis.set_title("vl" + str(anchor["sourceLayer"]))
        axis.set_xlim(1, 40)
        axis.set_ylim(-65, 15)
        axis.set_xlabel("Harmonic number (known exported f0)")
        axis.grid(alpha=.2)
        axis.legend()
    axes[0].set_ylabel("Band RMS amplitude relative to fundamental (dB)")
    figure.tight_layout()
    figure.savefig(output / "anchor-harmonics.png", dpi=130)
    plt.close(figure)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-root", type=Path, default=ROOT / ".context/sax-reference-downloads/vcsl")
    parser.add_argument("--analysis-report", type=Path, required=True,
                        help="Canonical cycle_reference_study report containing both source identities")
    parser.add_argument("--output", type=Path, default=ROOT / ".context/sax-anchor-analysis")
    parser.add_argument("--plot", action="store_true")
    args = parser.parse_args(argv)
    report_path = args.analysis_report.resolve()
    report = json.loads(report_path.read_text())
    observations = {entry["id"]: entry for entry in report["observations"]}
    manifest_path = args.reference_root / "download-manifest.json"
    manifest = json.loads(manifest_path.read_text())
    recordings = {entry["id"]: entry for entry in manifest["audioFiles"]}
    result = {"schema": "cycle-saxophone-anchor-analysis.v1", "purpose": "Developmental authoring targets; no held-out or taste claims",
              "method": {"pitch": "Existing canonical effective pitchFrames; no new detector; frequencyHz is authoritative over export octave convention",
                         "levels": "Original stereo channel energy, no gain matching, no crop normalization",
                         "fftSize": FFT_SIZE, "hopSeconds": HOP_SECONDS, "window": "Hann, centered; zero padding at file boundaries",
                         "harmonicCount": HARMONICS, "harmonicHalfBandBins": 2,
                         "harmonicAmplitude": "sqrt(2 * summed one-sided band mean-square power), then stage-averaged power",
                         "remainingBand": "Complement of first64 harmonic ±2FFT-bin bands; includes noise, room, unresolved modulation, FFT leakage, and higher partials; not separated broadband noise",
                         "comparison": "Compare source/synth band ratios and absolute channel-energy RMS, then one global gain-matched A/B; no local normalization or waveform-correlation optimization"},
              "analysisReportPath": str(report_path), "analysisReportSha256": file_digest(report_path),
              "anchors": []}
    data = []
    for identifier, design in ANCHORS.items():
        observed, recording = observations[identifier], recordings[identifier]
        analysis_path = report_path.parent / observed["analysisPath"]
        if file_digest(analysis_path) != observed["analysisSha256"]:
            raise ValueError("Saved analysis was changed")
        if recording["sha256"] != observed["sourceAudioSha256"]:
            raise ValueError("Downloaded recording and pitch evidence are not identical")
        measured, arrays = analyze_anchor((args.reference_root / recording["localPath"]).resolve(),
                                          json.loads(analysis_path.read_text()), analysis_path,
                                          recording["sha256"], design)
        result["anchors"].append(measured)
        data.append(arrays)
    args.output.mkdir(parents=True, exist_ok=True)
    destination = args.output / "anchor-analysis.json"
    destination.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    if args.plot:
        plot(result, data, args.output)
    print(destination.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
