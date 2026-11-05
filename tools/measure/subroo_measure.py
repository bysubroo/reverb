#!/usr/bin/env python3
"""
SubRoo Rooms - measurement & pack tool

  sweep        generate an exponential sine sweep (+ sweep.json with its parameters)
  batch        deconvolve a folder of multichannel sweep recordings into IRs (one gain for all)
  new-pack     write a location.json template for a measured pack
  from-captures  build packs from impulse captures (e.g. made with the Altiverb capture script)

Requires: python 3.9+, numpy, scipy, soundfile   (pip install numpy scipy soundfile)
"""
import argparse, json, os, re, sys
import numpy as np
import soundfile as sf
from scipy.signal import fftconvolve


# --------------------------------------------------------------------------- sweep
def make_sweep(fs, seconds, f1=20.0, f2=None, level_db=-6.0, fade=0.05):
    f2 = f2 or 0.95 * fs / 2
    t = np.arange(int(seconds * fs)) / fs
    R = np.log(f2 / f1)
    x = np.sin(2 * np.pi * f1 * seconds / R * (np.exp(t * R / seconds) - 1.0))
    n = int(fade * fs)
    w = np.ones_like(x)
    w[:n] = 0.5 - 0.5 * np.cos(np.pi * np.arange(n) / n)
    w[-n:] = w[:n][::-1]
    return (x * w * 10 ** (level_db / 20)).astype(np.float64), dict(f1=f1, f2=f2, seconds=seconds, fs=fs, level_db=level_db)


def inverse_filter(sweep, info):
    fs, T, f1, f2 = info["fs"], info["seconds"], info["f1"], info["f2"]
    R = np.log(f2 / f1)
    t = np.arange(len(sweep)) / fs
    inv = sweep[::-1] * np.exp(-t * R / T)            # Farina: amplitude compensation
    # normalise so that sweep * inv has unity gain in the pass band (200 Hz - 8 kHz)
    ref = fftconvolve(sweep, inv)
    spec = np.abs(np.fft.rfft(ref))
    freqs = np.fft.rfftfreq(len(ref), 1.0 / fs)
    band = (freqs > 200) & (freqs < 8000)
    return inv / np.median(spec[band])


def cmd_sweep(a):
    x, info = make_sweep(a.fs, a.seconds, level_db=a.level)
    silence = np.zeros(int(a.tail * a.fs))
    one = np.concatenate([x, silence])
    out = np.tile(one, a.repeats)
    sf.write(a.out, out, a.fs, subtype="PCM_24")
    info.update(repeats=a.repeats, tail=a.tail, file=os.path.basename(a.out))
    with open(os.path.splitext(a.out)[0] + ".json", "w") as f:
        json.dump(info, f, indent=2)
    print(f"sweep written: {a.out}  ({len(out)/a.fs:.1f} s, {a.repeats} x)")


# --------------------------------------------------------------------------- deconvolution
def deconvolve(rec, sweep, inv, info):
    """rec: (n, ch). Returns IR (m, ch) aligned so that index 0 == start of playback."""
    period = len(sweep) + int(info["tail"] * info["fs"])
    reps = max(1, info.get("repeats", 1))
    need = period * reps
    if rec.shape[0] < need:
        rec = np.pad(rec, ((0, need - rec.shape[0]), (0, 0)))
    avg = np.mean([rec[k * period:(k + 1) * period] for k in range(reps)], axis=0)
    out = []
    for c in range(avg.shape[1]):
        y = fftconvolve(avg[:, c], inv)
        out.append(y[len(sweep) - 1:len(sweep) - 1 + period - len(sweep)])
    return np.stack(out, axis=1)


def cmd_batch(a):
    info = json.load(open(a.sweep))
    sweep, _ = make_sweep(info["fs"], info["seconds"], info["f1"], info["f2"], info["level_db"])
    inv = inverse_filter(sweep, info)
    files = sorted(f for f in os.listdir(a.recordings) if f.lower().endswith(".wav"))
    if not files:
        sys.exit("no .wav files in " + a.recordings)
    irs, onsets = {}, []
    for f in files:
        rec, fs = sf.read(os.path.join(a.recordings, f), always_2d=True)
        if fs != info["fs"]:
            sys.exit(f"{f}: sample rate {fs} != sweep {info['fs']}")
        ir = deconvolve(rec, sweep, inv, info)
        env = np.max(np.abs(ir), axis=1)
        onsets.append(int(np.argmax(env > 0.2 * env.max())))
        irs[f] = ir
        print(f"  {f}: {rec.shape[1]} ch, peak {env.max():.4g}")
    # one common start (keeps relative delays) and ONE gain for the whole pack
    start = max(0, min(onsets) - int(0.001 * info["fs"]))
    length = int(a.length * info["fs"])
    peak = max(np.max(np.abs(ir[start:start + length])) for ir in irs.values())
    gain = 0.5 / peak if a.normalise else 1.0
    os.makedirs(a.out, exist_ok=True)
    fade = int(0.05 * info["fs"])
    for f, ir in irs.items():
        y = ir[start:start + length] * gain
        y[-fade:] *= np.linspace(1, 0, fade)[:, None]
        sf.write(os.path.join(a.out, f), y.astype(np.float32), info["fs"], subtype="FLOAT")
    print(f"{len(irs)} IRs -> {a.out}  (common gain {20*np.log10(gain):+.1f} dB, start {start} samples)")


# --------------------------------------------------------------------------- pack templates
RIGS = {
    "stereo": [{"name": "L", "az": 30}, {"name": "R", "az": -30}],
    "ab":     [{"name": "L", "az": 45}, {"name": "R", "az": -45}],
    "quad":   [{"name": "L", "az": 45}, {"name": "R", "az": -45}, {"name": "Ls", "az": 135}, {"name": "Rs", "az": -135}],
    "5.0":    [{"name": "L", "az": 30}, {"name": "R", "az": -30}, {"name": "C", "az": 0}, {"name": "Ls", "az": 110}, {"name": "Rs", "az": -110}],
}


def cmd_new_pack(a):
    os.makedirs(os.path.join(a.out, "ir"), exist_ok=True)
    loc = {
        "format": "subroo-location", "version": 1, "id": a.id, "name": a.name, "category": "Packs",
        "description": "Izmerena lokacija",
        "rooms": [{"id": "soba", "name": "Soba", "rect": [0, 0, 5, 4], "height": 2.7, "floor": "parquet", "walls": "plaster"},
                  {"id": "hodnik", "name": "Hodnik", "rect": [5, 0, 8, 4], "height": 2.6, "floor": "tile", "style": "tile"}],
        "portals": [{"id": "d_soba", "name": "Vrata sobe", "type": "door", "rooms": ["soba", "hodnik"], "at": [5, 2], "width": 0.9}],
        "sources": [{"id": "s1", "name": "Izvor 1", "pos": [1.5, 2.0, 1.6]}, {"id": "s2", "name": "Hodnik", "pos": [6.5, 2.0, 1.6]}],
        "mics": [{"id": "m1", "name": "Mikrofon 1", "pos": [3.5, 2.0, 1.6], "yaw": 180}],
        "defaults": {"source": "s1", "mic": "m1"},
        "pack": {"rig": RIGS[a.rig], "calibrationDb": 0, "irFolder": "ir"},
    }
    with open(os.path.join(a.out, "location.json"), "w", encoding="utf-8") as f:
        json.dump(loc, f, ensure_ascii=False, indent=1)
    print("template written:", os.path.join(a.out, "location.json"))


def slug(s):
    s = re.sub(r"[^a-zA-Z0-9]+", "_", s.strip().lower()).strip("_")
    return s or "x"


def cmd_from_captures(a):
    """
    Captures named  <Category>__<Room>__<Position>.wav  (written by the Altiverb capture script)
    become one location per room: one mic, one speaker spot per position.
    """
    groups = {}
    for f in sorted(os.listdir(a.captures)):
        if not f.lower().endswith(".wav"):
            continue
        parts = os.path.splitext(f)[0].split("__")
        if len(parts) < 2:
            continue
        cat, room = (parts[0], parts[1]) if len(parts) >= 3 else ("Altiverb", parts[0])
        pos = parts[2] if len(parts) >= 3 else parts[1]
        groups.setdefault((cat, room), []).append((pos, f))
    made = 0
    for (cat, room), items in groups.items():
        loc_id = "av_" + slug(room)
        d = os.path.join(a.out, f"{a.prefix}{room}")
        os.makedirs(os.path.join(d, "ir"), exist_ok=True)
        info = sf.info(os.path.join(a.captures, items[0][1]))
        rig = RIGS["quad"] if info.channels >= 4 else RIGS["stereo"]
        n = len(items)
        sources = []
        for i, (pos, f) in enumerate(items):
            ang = np.pi / 2 + (i - (n - 1) / 2) * (np.pi / max(2, n))
            sid = "s_" + slug(pos)
            sources.append({"id": sid, "name": pos, "pos": [3 + 2.0 * np.cos(ang), 2.5 + 1.6 * np.sin(ang), 1.6]})
            data, fs = sf.read(os.path.join(a.captures, f), always_2d=True)
            sf.write(os.path.join(d, "ir", f"{sid}__m_main__-.wav"), data.astype(np.float32), fs, subtype="FLOAT")
        loc = {
            "format": "subroo-location", "version": 1, "id": loc_id, "name": room, "category": f"{a.category} – {cat}",
            "description": f"Snimljeno iz Altiverba ({cat}). Pozicije su šematski raspoređene.",
            "rooms": [{"id": "room", "name": room, "rect": [0, 0, 6, 5], "height": 2.7}],
            "sources": [dict(s, room="room") for s in sources],
            "mics": [{"id": "m_main", "name": "Mikrofon", "pos": [3, 1.2, 1.6], "yaw": 90, "room": "room"}],
            "defaults": {"source": sources[0]["id"], "mic": "m_main"},
            "pack": {"rig": rig, "calibrationDb": a.calibration, "irFolder": "ir"},
        }
        with open(os.path.join(d, "location.json"), "w", encoding="utf-8") as f:
            json.dump(loc, f, ensure_ascii=False, indent=1)
        made += 1
        print(f"  {room}: {n} positions, {info.channels} ch")
    print(f"{made} packs -> {a.out}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = p.add_subparsers(dest="cmd", required=True)
    q = s.add_parser("sweep"); q.add_argument("--out", default="sweep.wav"); q.add_argument("--fs", type=int, default=48000)
    q.add_argument("--seconds", type=float, default=10.0); q.add_argument("--tail", type=float, default=4.0)
    q.add_argument("--repeats", type=int, default=1); q.add_argument("--level", type=float, default=-6.0)
    q = s.add_parser("batch"); q.add_argument("--sweep", required=True); q.add_argument("--recordings", required=True)
    q.add_argument("--out", required=True); q.add_argument("--length", type=float, default=3.5)
    q.add_argument("--normalise", action="store_true", help="scale the whole batch by one common gain")
    q = s.add_parser("new-pack"); q.add_argument("--id", required=True); q.add_argument("--name", required=True)
    q.add_argument("--out", required=True); q.add_argument("--rig", choices=list(RIGS), default="quad")
    q = s.add_parser("from-captures"); q.add_argument("--captures", required=True); q.add_argument("--out", required=True)
    q.add_argument("--category", default="Altiverb"); q.add_argument("--prefix", default="AV - ")
    q.add_argument("--calibration", type=float, default=0.0)
    a = p.parse_args()
    {"sweep": cmd_sweep, "batch": cmd_batch, "new-pack": cmd_new_pack, "from-captures": cmd_from_captures}[a.cmd](a)


if __name__ == "__main__":
    main()
