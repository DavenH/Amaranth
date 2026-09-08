#!/usr/bin/env python3

"""Deterministic waveform, spectrum, and cyclogram comparison for Cycle WAVs."""

import cmath
import hashlib
import math
import struct
import wave
from pathlib import Path


def read_wav(path):
    with wave.open(str(path), "rb") as source:
        channel_count = source.getnchannels()
        sample_rate = source.getframerate()
        sample_width = source.getsampwidth()
        frame_count = source.getnframes()
        encoded = source.readframes(frame_count)

    if sample_width == 1:
        values = [(value - 128) / 128.0 for value in encoded]
    elif sample_width == 2:
        count = frame_count * channel_count
        values = [value / 32768.0 for value in struct.unpack(f"<{count}h", encoded)]
    elif sample_width == 3:
        values = []
        for offset in range(0, len(encoded), 3):
            value = int.from_bytes(encoded[offset:offset + 3], "little", signed=True)
            values.append(value / 8388608.0)
    elif sample_width == 4:
        count = frame_count * channel_count
        values = [value / 2147483648.0 for value in struct.unpack(f"<{count}i", encoded)]
    else:
        raise ValueError(f"Unsupported WAV sample width: {sample_width}")

    channels = [values[index::channel_count] for index in range(channel_count)]
    return {
        "sampleRate": sample_rate,
        "sampleWidth": sample_width,
        "samplePayloadSha256": hashlib.sha256(encoded).hexdigest(),
        "channels": channels,
        "frames": frame_count,
    }


def read_raw_f32(path, sample_rate, channel_count, frame_count):
    encoded = Path(path).read_bytes()
    expected_size = channel_count * frame_count * 4
    if len(encoded) != expected_size:
        raise ValueError(
            f"Raw float capture has {len(encoded)} bytes; expected {expected_size}")
    values = struct.unpack(f"<{channel_count * frame_count}f", encoded)
    channels = [
        list(values[channel * frame_count:(channel + 1) * frame_count])
        for channel in range(channel_count)
    ]
    return {
        "sampleRate": sample_rate,
        "sampleWidth": 4,
        "samplePayloadSha256": hashlib.sha256(encoded).hexdigest(),
        "channels": channels,
        "frames": frame_count,
    }


def exact_sample_comparison(reference, candidate):
    metadata_equal = (
        reference["sampleRate"] == candidate["sampleRate"]
        and reference["sampleWidth"] == candidate["sampleWidth"]
        and len(reference["channels"]) == len(candidate["channels"])
        and reference["frames"] == candidate["frames"]
    )
    differing_samples = 0
    first_mismatch = None
    if metadata_equal:
        for channel, (left, right) in enumerate(zip(
                reference["channels"], candidate["channels"])):
            for frame, (reference_sample, candidate_sample) in enumerate(zip(left, right)):
                if reference_sample == candidate_sample:
                    continue
                differing_samples += 1
                if first_mismatch is None:
                    first_mismatch = {
                        "channel": channel,
                        "frame": frame,
                        "reference": reference_sample,
                        "candidate": candidate_sample,
                    }
    return {
        "metadataEqual": metadata_equal,
        "samplesEqual": metadata_equal and differing_samples == 0,
        "referencePayloadSha256": reference["samplePayloadSha256"],
        "candidatePayloadSha256": candidate["samplePayloadSha256"],
        "differingSamples": differing_samples if metadata_equal else None,
        "firstMismatch": first_mismatch,
    }


def mixdown(channels):
    if len(channels) == 1:
        return list(channels[0])
    scale = 1.0 / len(channels)
    return [sum(frame) * scale for frame in zip(*channels)]


def rms(values):
    return math.sqrt(sum(value * value for value in values) / max(1, len(values)))


def normalized_difference(reference, candidate):
    difference = [left - right for left, right in zip(reference, candidate)]
    return rms(difference) / max(rms(reference), 1.0e-12)


def correlation_at_lag(reference, candidate, lag):
    reference_start = max(0, -lag)
    candidate_start = max(0, lag)
    count = min(
        len(reference) - reference_start,
        len(candidate) - candidate_start,
    )
    if count <= 0:
        return None

    left = reference[reference_start:reference_start + count]
    right = candidate[candidate_start:candidate_start + count]
    cross = sum(a * b for a, b in zip(left, right))
    left_energy = sum(value * value for value in left)
    right_energy = sum(value * value for value in right)
    correlation = cross / math.sqrt(max(left_energy * right_energy, 1.0e-24))
    return correlation, left, right


def alignment(reference, candidate, maximum_lag, ambiguity_tolerance=1.0e-4):
    candidates = []
    for lag in range(-maximum_lag, maximum_lag + 1):
        compared = correlation_at_lag(reference, candidate, lag)
        if compared is not None:
            candidates.append((compared[0], lag, compared[1], compared[2]))

    maximum_correlation = max(candidate[0] for candidate in candidates)
    equivalent = [
        candidate for candidate in candidates
        if candidate[0] >= maximum_correlation - ambiguity_tolerance
    ]
    correlation, lag, left, right = min(equivalent, key=lambda candidate: abs(candidate[1]))
    return {
        "lagSamples": lag,
        "correlation": correlation,
        "maximumCorrelation": maximum_correlation,
        "reference": left,
        "candidate": right,
    }


def fit_gain(reference, candidate):
    candidate_energy = sum(value * value for value in candidate)
    if candidate_energy <= 1.0e-24:
        return 0.0
    return sum(left * right for left, right in zip(reference, candidate)) / candidate_energy


def fft(values):
    count = len(values)
    if count == 0 or count & (count - 1):
        raise ValueError("FFT length must be a non-zero power of two")

    result = [complex(value) for value in values]
    target = 0
    for source in range(1, count):
        bit = count >> 1
        while target & bit:
            target ^= bit
            bit >>= 1
        target ^= bit
        if source < target:
            result[source], result[target] = result[target], result[source]

    width = 2
    while width <= count:
        root = cmath.exp(-2j * math.pi / width)
        for start in range(0, count, width):
            factor = 1.0 + 0j
            half = width // 2
            for index in range(start, start + half):
                even = result[index]
                odd = result[index + half] * factor
                result[index] = even + odd
                result[index + half] = even - odd
                factor *= root
        width *= 2
    return result


def average_spectrum(signal, fft_size=2048, hop_size=512):
    if len(signal) < fft_size:
        signal = list(signal) + [0.0] * (fft_size - len(signal))
    window = [
        0.5 - 0.5 * math.cos(2.0 * math.pi * index / (fft_size - 1))
        for index in range(fft_size)
    ]
    powers = [0.0] * (fft_size // 2 + 1)
    frame_count = 0
    for start in range(0, len(signal) - fft_size + 1, hop_size):
        transformed = fft([
            signal[start + index] * window[index]
            for index in range(fft_size)
        ])
        for index in range(len(powers)):
            powers[index] += abs(transformed[index]) ** 2
        frame_count += 1
    return [math.sqrt(value / max(1, frame_count)) for value in powers]


def spectrum_difference(reference, candidate, floor_db=-80.0):
    reference_peak = max(reference, default=0.0)
    floor = max(reference_peak * 10.0 ** (floor_db / 20.0), 1.0e-12)
    reference_db = [20.0 * math.log10(max(value, floor)) for value in reference]
    candidate_db = [20.0 * math.log10(max(value, floor)) for value in candidate]
    differences = [left - right for left, right in zip(reference_db, candidate_db)]
    return math.sqrt(sum(value * value for value in differences) / len(differences))


def midi_frequency(note):
    return 440.0 * 2.0 ** ((note - 69) / 12.0)


def tone_amplitude(signal, sample_rate, frequency):
    if not signal:
        return 0.0
    window = [
        0.5 - 0.5 * math.cos(2.0 * math.pi * index / max(1, len(signal) - 1))
        for index in range(len(signal))
    ]
    projection = sum(
        sample * weight * cmath.exp(-2j * math.pi * frequency * index / sample_rate)
        for index, (sample, weight) in enumerate(zip(signal, window))
    )
    return 2.0 * abs(projection) / max(sum(window), 1.0e-12)


def pitch_signature(signal, sample_rate, midi_note):
    frequency = midi_frequency(midi_note)
    amplitudes = {
        "halfFrequency": tone_amplitude(signal, sample_rate, frequency * 0.5),
        "fundamental": tone_amplitude(signal, sample_rate, frequency),
        "secondHarmonic": tone_amplitude(signal, sample_rate, frequency * 2.0),
    }
    fundamental = max(amplitudes["fundamental"], 1.0e-12)
    amplitudes["halfToFundamental"] = amplitudes["halfFrequency"] / fundamental
    amplitudes["secondToFundamental"] = amplitudes["secondHarmonic"] / fundamental
    return amplitudes


def harmonic_profile(signal, sample_rate, midi_note, harmonic_count=16):
    frequency = midi_frequency(midi_note)
    window = [
        0.5 - 0.5 * math.cos(2.0 * math.pi * index / max(1, len(signal) - 1))
        for index in range(len(signal))
    ]
    normalization = max(sum(window), 1.0e-12)
    amplitudes = []
    for harmonic in range(1, harmonic_count + 1):
        harmonic_frequency = frequency * harmonic
        if harmonic_frequency >= sample_rate * 0.5:
            break
        projection = sum(
            sample * weight * cmath.exp(-2j * math.pi * harmonic_frequency * index / sample_rate)
            for index, (sample, weight) in enumerate(zip(signal, window))
        )
        amplitudes.append(2.0 * abs(projection) / normalization)
    fundamental = max(amplitudes[0] if amplitudes else 0.0, 1.0e-12)
    return [amplitude / fundamental for amplitude in amplitudes]


def interpolate(signal, position):
    lower = int(math.floor(position))
    fraction = position - lower
    if lower < 0 or lower + 1 >= len(signal):
        return 0.0
    return signal[lower] + fraction * (signal[lower + 1] - signal[lower])


def cyclogram(signal, sample_rate, midi_note, phase_bins=512, row_count=16):
    period = sample_rate / midi_frequency(midi_note)
    available_rows = max(0, int((len(signal) - 1) / period))
    row_count = min(row_count, available_rows)
    rows = []
    for row_index in range(row_count):
        start = row_index * period
        rows.append([
            interpolate(signal, start + period * phase / phase_bins)
            for phase in range(phase_bins)
        ])
    return rows


def cyclogram_metrics(rows):
    if not rows:
        return {"rows": 0, "meanNormalizedRowError": None, "maxNormalizedRowError": None}, []
    mean_row = [sum(values) / len(rows) for values in zip(*rows)]
    denominator = max(rms(mean_row), 1.0e-12)
    errors = [rms([a - b for a, b in zip(row, mean_row)]) / denominator for row in rows]
    return {
        "rows": len(rows),
        "meanNormalizedRowError": sum(errors) / len(errors),
        "maxNormalizedRowError": max(errors),
    }, mean_row


def analyze_pair(
        reference_wav,
        candidate_wav,
        midi_note,
        analysis_start_ms=250.0,
        analysis_duration_ms=500.0,
        maximum_lag=512):
    reference_data = read_wav(reference_wav)
    candidate_data = read_wav(candidate_wav)
    if reference_data["sampleRate"] != candidate_data["sampleRate"]:
        raise ValueError("WAV sample rates differ")
    if len(reference_data["channels"]) != len(candidate_data["channels"]):
        raise ValueError("WAV channel counts differ")

    sample_rate = reference_data["sampleRate"]
    start = round(analysis_start_ms * sample_rate / 1000.0)
    count = round(analysis_duration_ms * sample_rate / 1000.0)
    reference = mixdown(reference_data["channels"])[start:start + count]
    candidate = mixdown(candidate_data["channels"])[start:start + count]
    aligned = alignment(reference, candidate, maximum_lag)
    gain = fit_gain(aligned["reference"], aligned["candidate"])
    gain_matched = [gain * value for value in aligned["candidate"]]

    reference_spectrum = average_spectrum(aligned["reference"])
    candidate_spectrum = average_spectrum(gain_matched)
    reference_harmonics = harmonic_profile(aligned["reference"], sample_rate, midi_note)
    candidate_harmonics = harmonic_profile(aligned["candidate"], sample_rate, midi_note)
    reference_cyclogram, reference_mean = cyclogram_metrics(cyclogram(
        aligned["reference"], sample_rate, midi_note))
    candidate_cyclogram, candidate_mean = cyclogram_metrics(cyclogram(
        gain_matched, sample_rate, midi_note))

    return {
        "sampleRate": sample_rate,
        "channels": len(reference_data["channels"]),
        "midiNote": midi_note,
        "fundamentalHz": midi_frequency(midi_note),
        "analysisStartMs": analysis_start_ms,
        "analysisDurationMs": analysis_duration_ms,
        "exact": exact_sample_comparison(reference_data, candidate_data),
        "unaligned": {
            "referenceRms": rms(reference),
            "candidateRms": rms(candidate),
            "normalizedResidual": normalized_difference(reference, candidate),
        },
        "alignment": {
            "candidateLagSamples": aligned["lagSamples"],
            "correlation": aligned["correlation"],
            "maximumCorrelation": aligned["maximumCorrelation"],
        },
        "gainFit": {
            "candidateScale": gain,
            "candidateScaleDb": 20.0 * math.log10(max(abs(gain), 1.0e-12)),
            "normalizedResidual": normalized_difference(aligned["reference"], gain_matched),
        },
        "spectrum": {
            "logMagnitudeRmseDb": spectrum_difference(reference_spectrum, candidate_spectrum),
            "referencePitchSignature": pitch_signature(
                aligned["reference"], sample_rate, midi_note),
            "candidatePitchSignature": pitch_signature(
                aligned["candidate"], sample_rate, midi_note),
            "referenceHarmonicRatios": reference_harmonics,
            "candidateHarmonicRatios": candidate_harmonics,
            "harmonicRatioRmsDifference": rms([
                left - right
                for left, right in zip(reference_harmonics, candidate_harmonics)
            ]),
        },
        "cyclogram": {
            "reference": reference_cyclogram,
            "candidate": candidate_cyclogram,
            "meanRowNormalizedDifference": normalized_difference(reference_mean, candidate_mean),
        },
    }
