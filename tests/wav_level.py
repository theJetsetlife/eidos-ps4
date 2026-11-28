#!/usr/bin/env python3
"""
wav_level.py -- does a recording contain sustained audio after a point?

    python tests/wav_level.py FILE.wav --after SECONDS [--min-rms N] [--min-share F]
                              [--min-duration SECONDS]

Each eligible 10-second span needs a share of 100-ms windows whose RMS level
reaches N (default 64).  At least F (default 0.5) of the eligible spans must
meet that condition.  This rejects short impulses and long silent periods; it
cannot identify the music or distinguish it from sustained sound effects.
Standard library only; 16-bit PCM WAV as eidos_boot writes it
(EIDOS_AUDIO_WAV).
"""

import argparse
import array
import math
import sys
import wave


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("wav")
    ap.add_argument("--after", type=float, required=True)
    ap.add_argument("--min-rms", type=int, default=64)
    ap.add_argument("--min-share", type=float, default=0.5)
    ap.add_argument("--min-window-share", type=float, default=0.25)
    ap.add_argument("--min-duration", type=float, default=0)
    a = ap.parse_args(argv)
    if not math.isfinite(a.after) or a.after < 0:
        ap.error("--after must be a finite nonnegative number")
    if not 1 <= a.min_rms <= 32768:
        ap.error("--min-rms must be between 1 and 32768")
    if not 0 < a.min_share <= 1 or not 0 < a.min_window_share <= 1:
        ap.error("shares must be greater than zero and at most one")
    if not math.isfinite(a.min_duration) or a.min_duration < 0:
        ap.error("--min-duration must be a finite nonnegative number")

    with wave.open(a.wav) as w:
        if w.getsampwidth() != 2:
            print("not 16-bit PCM")
            return 1
        rate, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
        data = w.readframes(n)
    if rate < 10 or ch < 1 or len(data) != n * ch * 2:
        print("invalid or truncated PCM data")
        return 1
    samples = array.array("h")
    samples.frombytes(data)
    if sys.byteorder != "little":
        samples.byteswap()
    span = int(rate * ch * 10)
    window = (rate // 10) * ch
    spans_after = 0
    loud_after = 0
    line = []
    for i in range(0, len(samples), span):
        block = samples[i:i + span]
        peak = max((abs(x) for x in block), default=0)
        t = i / (rate * ch)
        active = windows = 0
        for j in range(0, len(block), window):
            part = block[j:j + window]
            # Ignore a fractional final window shorter than 50 ms.
            if len(part) < window // 2:
                continue
            windows += 1
            if sum(x * x for x in part) >= a.min_rms * a.min_rms * len(part):
                active += 1
        line.append("%ds:peak=%d active=%d/%d" % (t, peak, active, windows))
        if t >= a.after and len(block) >= span // 2:
            spans_after += 1
            if windows and active >= a.min_window_share * windows:
                loud_after += 1
    print("audio %.1f s; 10-second spans: %s" % (n / rate, " ".join(line)))
    if n / rate <= a.after:
        print("the recording ends before %.0f s" % a.after)
        return 1
    if n / rate < a.min_duration:
        print("the recording is shorter than the required %.1f s" % a.min_duration)
        return 1
    if spans_after == 0 or loud_after < a.min_share * spans_after:
        print("insufficient sustained audio after %.0f s (%d of %d spans meet the RMS requirement)"
              % (a.after, loud_after, spans_after))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
