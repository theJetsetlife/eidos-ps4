#!/usr/bin/env python3
"""test_wav_level.py -- tests/wav_level.py, the silent-audio check."""

import os
import struct
import subprocess
import sys
import tempfile
import unittest
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "wav_level.py")
RATE = 8000


def write(path, spans):
    """spans: peak per 10 s, 2 channels, 16 bit."""
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        for peak in spans:
            frames = struct.pack("<hh", peak, -peak) * (RATE * 10)
            w.writeframes(frames)


def run(spans, after, min_rms=64, min_duration=0):
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "a.wav")
        write(path, spans)
        p = subprocess.run([sys.executable, "-I", TOOL, path, "--after", str(after),
                            "--min-rms", str(min_rms), "--min-duration", str(min_duration)],
                           capture_output=True, text=True)
        return p.returncode, p.stdout


def run_sparse(step_frames, length_frames):
    """Repeat a short pulse in each 10-second span after 30 seconds."""
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "sparse.wav")
        with wave.open(path, "wb") as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(b"\0" * (RATE * 30 * 4))
            block = bytearray(RATE * 10 * 4)
            pulse = struct.pack("<hh", 8000, -8000) * length_frames
            for frame in range(0, RATE * 10, step_frames):
                block[frame * 4:frame * 4 + len(pulse)] = pulse
            for _ in range(4):
                w.writeframes(block)
        p = subprocess.run([sys.executable, "-I", TOOL, path, "--after", "30",
                            "--min-rms", "1000"], capture_output=True, text=True)
        return p.returncode, p.stdout


class WavLevel(unittest.TestCase):
    def test_sound_after_the_point_passes(self):
        self.assertEqual(run([9000, 0, 0, 7000], 30)[0], 0)

    def test_silence_from_the_point_on_fails(self):
        # The intro's observed shape: audible output, then silence.
        rc, out = run([15240, 9485, 14764, 0, 0, 0, 0], 30)
        self.assertEqual(rc, 1)
        self.assertIn("insufficient sustained audio after 30 s", out)

    def test_quiet_noise_below_the_threshold_counts_as_silence(self):
        self.assertEqual(run([9000, 9000, 9000, 10, 12], 30)[0], 1)

    def test_rare_loud_spans_fail(self):
        # one loud span among silent ones: the old, partial behaviour
        rc, out = run([9000, 9000, 9000, 0, 0, 9000, 0, 0, 0, 0], 30)
        self.assertEqual(rc, 1)
        self.assertIn("1 of 7 spans", out)

    def test_one_sample_per_span_does_not_count_as_audio(self):
        self.assertEqual(run_sparse(RATE * 10, 1)[0], 1)

    def test_one_sample_per_window_does_not_count_as_audio(self):
        self.assertEqual(run_sparse(RATE // 10, 1)[0], 1)

    def test_brief_loud_pulse_per_span_does_not_count_as_sustained(self):
        self.assertEqual(run_sparse(RATE * 10, RATE // 10)[0], 1)

    def test_steady_signal_throughout_passes(self):
        self.assertEqual(run([9000] * 10, 30)[0], 0)

    def test_a_recording_shorter_than_the_point_fails(self):
        rc, out = run([9000, 9000], 30)
        self.assertEqual(rc, 1)
        self.assertIn("ends before", out)

    def test_audio_must_reach_the_required_duration(self):
        rc, out = run([9000] * 6, 30, min_duration=80)
        self.assertEqual(rc, 1)
        self.assertIn("shorter than the required 80.0 s", out)


if __name__ == "__main__":
    unittest.main()
