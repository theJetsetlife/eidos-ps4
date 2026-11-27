#!/usr/bin/env python3
"""
wav_level.py -- is a recording of the game's audio silent after a point?

    python tests/wav_level.py FILE.wav --after SECONDS [--min-peak N] [--min-share F]

Prints the peak of each 10-second span and fails (exit 1) when fewer than
the share F (default 0.5) of the spans from SECONDS on reach a peak of N
(default 64): the game's music and effects stopped, or are only heard in
stray bursts.  Standard library only; 16-bit PCM WAV as eidos_boot
writes it (EIDOS_AUDIO_WAV).
"""

import argparse
import struct
import sys
import wave


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("wav")
    ap.add_argument("--after", type=float, required=True)
    ap.add_argument("--min-peak", type=int, default=64)
    ap.add_argument("--min-share", type=float, default=0.5)
    a = ap.parse_args(argv)

    with wave.open(a.wav) as w:
        if w.getsampwidth() != 2:
            print("not 16-bit PCM")
            return 1
        rate, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
        data = w.readframes(n)
    samples = struct.unpack("<%dh" % (len(data) // 2), data)
    span = int(rate * ch * 10)
    spans_after = 0
    loud_after = 0
    line = []
    for i in range(0, len(samples), span):
        block = samples[i:i + span]
        peak = max((abs(x) for x in block), default=0)
        t = i / (rate * ch)
        line.append("%ds:%d" % (t, peak))
        if t >= a.after and len(block) >= span // 2:
            spans_after += 1
            if peak >= a.min_peak:
                loud_after += 1
    print("audio %.1f s; peak per 10 s: %s" % (n / rate, " ".join(line)))
    if n / rate <= a.after:
        print("the recording ends before %.0f s" % a.after)
        return 1
    if spans_after == 0 or loud_after < a.min_share * spans_after:
        print("silent from %.0f s on (%d of %d spans reach %d)"
              % (a.after, loud_after, spans_after, a.min_peak))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
