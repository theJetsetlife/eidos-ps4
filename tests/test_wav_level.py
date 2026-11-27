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


def run(spans, after, min_peak=64):
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "a.wav")
        write(path, spans)
        p = subprocess.run([sys.executable, "-I", TOOL, path, "--after", str(after),
                            "--min-peak", str(min_peak)], capture_output=True, text=True)
        return p.returncode, p.stdout


class WavLevel(unittest.TestCase):
    def test_sound_after_the_point_passes(self):
        self.assertEqual(run([9000, 0, 0, 7000], 30)[0], 0)

    def test_silence_from_the_point_on_fails(self):
        # the intro's shape: music, then nothing for the rest of the run
        rc, out = run([15240, 9485, 14764, 0, 0, 0, 0], 30)
        self.assertEqual(rc, 1)
        self.assertIn("silent from 30 s on", out)

    def test_quiet_noise_below_the_threshold_counts_as_silence(self):
        self.assertEqual(run([9000, 9000, 9000, 10, 12], 30)[0], 1)

    def test_a_recording_shorter_than_the_point_fails(self):
        rc, out = run([9000, 9000], 30)
        self.assertEqual(rc, 1)
        self.assertIn("ends before", out)


if __name__ == "__main__":
    unittest.main()
