"""Explicit time/level policy and channel-energy descriptors over Cycle diagnostics."""

import math

from cycle_audio_diff import alignment, average_spectrum, midi_frequency, read_wav, rms
from cycle_reference_study import file_digest


FFT_SIZE, HOP_SIZE = 2048, 512
DESCRIPTORS = ("rms", "centroidHz", "flatness", "harmonicBandFraction", "upperHarmonicBandFraction", "remainingBandFraction")


def level(channels):
    channel_rms = [rms(channel) for channel in channels]
    return {"rms": math.sqrt(sum(value * value for value in channel_rms) / len(channels)),
            "channelRms": channel_rms, "peak": max(abs(value) for channel in channels for value in channel)}


def read_clip(source):
    if file_digest(source["audioPath"]) != source["audioSha256"]:
        raise ValueError("Audio changed after plan resolution")
    decoded = read_wav(source["audioPath"])
    rate = source["sampleRate"]
    start, end = [round(source["crop"][key] * rate) for key in ("startSeconds", "endSeconds")]
    channels = [channel[start:end] for channel in decoded["channels"]]
    if not channels[0] or any(not math.isfinite(value) for channel in channels for value in channel):
        raise ValueError("Empty or non-finite audio")
    return channels


def pitch_evidence(case):
    pitch = case.get("pitch")
    if pitch is None:
        return None, "No fixed-pitch monophonic evidence declared"
    if pitch["origin"] != "observed" or pitch["confidence"] < 0.8 or not pitch["monophonic"]:
        return None, "Pitch evidence is inferred, uncertain or not monophonic"
    if any(case[side]["lowConfidence"] or case[side]["trackingUsedFallback"] for side in ("a", "b")):
        return None, "Reference tracking is insecure or used fallback"
    if midi_frequency(pitch["midiNote"]) <= 3 * case["a"]["sampleRate"] / FFT_SIZE:
        return None, "Pitch is below harmonic-band resolution: fundamental <= 3 FFT bins"
    return pitch["midiNote"], "Observed fixed-pitch monophonic evidence, confidence >= 0.8"


def trace(channels, rate, start, end, note):
    result = []
    fundamental = midi_frequency(note) if note is not None else None
    for offset in range(start, end, HOP_SIZE):
        frames = [channel[offset:min(offset + FFT_SIZE, end)] for channel in channels]
        spectra = [average_spectrum(frame, FFT_SIZE, FFT_SIZE) for frame in frames]
        powers = [sum(values) / len(spectra) for values in zip(*[
            [magnitude * magnitude for magnitude in spectrum] for spectrum in spectra])]
        total = sum(powers)
        energy = level(frames)["rms"]
        centroid = sum(index * rate / FFT_SIZE * power for index, power in enumerate(powers)) / max(total, 1e-24)
        flatness = math.exp(sum(math.log(max(power, 1e-24)) for power in powers) / len(powers)) / max(total / len(powers), 1e-24)
        row = {"timeSeconds": (offset + len(frames[0]) / 2) / rate, "rms": energy,
               "centroidHz": centroid, "flatness": min(1.0, flatness) if total > 1e-20 else 0.0}
        if fundamental is not None:
            bands = [0.0] * 17
            for index, power in enumerate(powers):
                harmonic = round(index * rate / FFT_SIZE / fundamental)
                if 1 <= harmonic <= 16 and abs(index * rate / FFT_SIZE - harmonic * fundamental) <= 1.5 * rate / FFT_SIZE:
                    bands[harmonic] += power
            harmonic = sum(bands)
            row["harmonicBandFraction"] = harmonic / max(total, 1e-24)
            row["upperHarmonicBandFraction"] = sum(bands[2:]) / max(total, 1e-24)
            row["remainingBandFraction"] = max(0, total - harmonic) / max(total, 1e-24)
        result.append(row)
    return result


def differences(left, right):
    return {key: sum(abs(a[key] - b[key]) for a, b in zip(left, right)) / len(left)
            for key in DESCRIPTORS if key in left[0]}


def fixed_lag(channels, rate, policy):
    if policy is None:
        return {"mode": "none", "lagSamples": 0}
    start, end = [round(policy["calibration"][key] * rate) for key in ("startSeconds", "endSeconds")]
    selected = [max(range(len(side)), key=lambda channel: rms(side[channel][start:end])) for side in channels]
    samples = [side[index][start:end] for side, index in zip(channels, selected)]
    maximum = round(policy["maxLagMs"] * rate / 1000)
    if min(map(rms, samples)) < 1e-10 or (end - start) * (2 * maximum + 1) > 8_000_000:
        raise ValueError("Calibration is silent or exceeds 8 million correlation sample pairs")
    matched = alignment(*samples, maximum)
    if matched["correlation"] < 0.5:
        raise ValueError("Calibration has insufficient positive correlation")
    return {"mode": "fixedLag", "lagSamples": matched["lagSamples"],
            "correlation": matched["correlation"], "maximumCorrelation": matched["maximumCorrelation"],
            "calibration": policy["calibration"], "channelIndices": selected,
            "maxLagMs": policy["maxLagMs"], "policy": "highest-energy channel per side; one lag for all intervals"}


def rms_gains(levels):
    if min(item["rms"] for item in levels) <= 1e-10:
        return {"available": False, "gains": [1.0, 1.0], "reason": "At least one crop is silent"}
    target = min(item["rms"] for item in levels)
    gains = [target / item["rms"] for item in levels]
    headroom = min(1, 0.999 / max(item["peak"] * gain for item, gain in zip(levels, gains)))
    return {"available": True, "gains": [gain * headroom for gain in gains],
            "targetRms": target * headroom, "policy": "one channel-energy RMS gain per full crop; attenuation only"}


def analyze_case(case, policy=None):
    channels = [read_clip(case[side]) for side in ("a", "b")]
    rate = case["a"]["sampleRate"]
    levels = [level(side) for side in channels]
    note, reason = pitch_evidence(case)
    calibrated = fixed_lag(channels, rate, policy)
    lag = calibrated["lagSamples"]
    overlap_start, overlap_end = max(0, -lag), min(len(channels[0][0]), len(channels[1][0]) - lag)
    calibrated["overlapSeconds"] = [overlap_start / rate, overlap_end / rate]
    calibrated["discardedSamples"] = {"a": {"leading": overlap_start, "trailing": len(channels[0][0]) - overlap_end},
                                        "b": {"leading": overlap_start + lag, "trailing": len(channels[1][0]) - overlap_end - lag}}
    phases, cache = [], {}
    for phase in case["intervals"]:
        start, end = [round(phase[key] * rate) for key in ("startSeconds", "endSeconds")]
        if end <= start:
            raise ValueError("Analysis interval contains no complete sample")
        def measured(side, first, stop):
            key = (side, first, stop, note)
            if key not in cache:
                cache[key] = trace(channels[side], rate, first, stop, note)
            return cache[key]
        raw = [measured(side, start, end) for side in range(2)]
        entry = {**phase, "raw": {"a": raw[0], "b": raw[1], "meanAbsoluteDifference": differences(*raw)}}
        if policy:
            first, stop = max(start, overlap_start), min(end, overlap_end)
            if stop <= first:
                entry["aligned"] = {"available": False, "reason": "No overlap for this interval"}
            else:
                left, right = measured(0, first, stop), measured(1, first + lag, stop + lag)
                right = [{**row, "timeSeconds": row["timeSeconds"] - lag / rate} for row in right]
                entry["aligned"] = {"available": True, "a": left, "b": right,
                                    "overlapSeconds": [first / rate, stop / rate],
                                    "meanAbsoluteDifference": differences(left, right)}
        phases.append(entry)
    evidence = {"id": case["id"], "partition": case["partition"], "sources": {side: case[side] for side in ("a", "b")},
                "pitch": {"declared": case.get("pitch"), "used": note is not None, "reason": reason},
                "levels": {"a": levels[0], "b": levels[1]}, "alignment": calibrated,
                "rmsMatching": rms_gains(levels), "intervals": phases}
    return evidence, channels
