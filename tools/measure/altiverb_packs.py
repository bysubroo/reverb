#!/usr/bin/env python3
"""
SubRoo Rooms - build location packs from Altiverb captures

The capture job (tools/altiverb-capture/SubRoo_Altiverb_Capture.lua) renders an impulse
through Altiverb for every IR in capture_list.csv and writes
    <category>__<room>__<position>__<C|L|R>.wav
C = mono-in IR (unity impulse), L / R = stereo-in IR driven from the left / right input.

This tool turns those captures into one pack per room:
  * polarity is flipped so Altiverb's direct sound is positive
  * every IR is trimmed at the end of its decay (no per-file normalisation: one gain for all)
  * stereo-in rooms get three sources: centre (L+R)/2, left and right
  * the "next door" houses get door / window state bits, so the plugin's door buttons
    pick the matching Altiverb recording (see NEXT_DOOR below)

usage:
  python altiverb_packs.py --captures AltiverbPOST --manifest manifest.csv --out Packs
"""
import argparse, csv, json, os, re, shutil, sys
import numpy as np
import soundfile as sf

RIG_STEREO = [{"name": "L", "az": 30}, {"name": "R", "az": -30}]

CATEGORY_NAMES = {
    "domestic": "Stan i kuća",
    "next door": "Kroz zid i vrata",
    "car": "Automobili",
    "aircraft-train & boat": "Avion, voz, brod",
    "outdoor & underground": "Napolju i pod zemljom",
    "corridor-stairs": "Hodnici i stepeništa",
    "public spaces": "Javni prostori",
    "school-office": "Škola i kancelarija",
    "misc": "Ostalo",
}


def lua_sanitize(s):
    """same rule as sanitize() in the capture script"""
    s = s.replace("&apos;", "").replace("&amp;", "and").replace("&quot;", "")
    s = re.sub(r"[^A-Za-z0-9\-\. ]", "_", s)
    s = re.sub(r"\s+", " ", s)
    return s.strip()


def slug(s):
    s = re.sub(r"[^a-zA-Z0-9]+", "_", s.strip().lower()).strip("_")
    return s or "x"


def load_capture(path):
    x, fs = sf.read(path, always_2d=True, dtype="float64")
    return -x[:, :2], fs          # Altiverb's output is polarity-inverted (direct = -1)


def trim(x, fs, floor_db=-75.0, min_s=0.25, fade_s=0.02):
    e = np.sum(x ** 2, axis=1)
    edc = np.cumsum(e[::-1])[::-1]
    if edc[0] <= 0:
        return x[: int(min_s * fs)]
    edc_db = 10 * np.log10(edc / edc[0] + 1e-30)
    end = int(np.argmax(edc_db < floor_db)) or len(x)
    end = max(end, int(min_s * fs))
    end = min(len(x), end + int(fade_s * fs))
    y = x[:end].copy()
    nf = min(int(fade_s * fs), end)
    y[end - nf:] *= np.linspace(1, 0, nf)[:, None]
    return y


def rt_estimate(x, fs):
    e = np.sum(x ** 2, axis=1)
    edc = np.cumsum(e[::-1])[::-1]
    edc_db = 10 * np.log10(edc / (edc[0] + 1e-30) + 1e-30)
    i5, i25 = np.argmax(edc_db < -5), np.argmax(edc_db < -25)
    if i25 <= i5:
        return 0.0
    return 3.0 * (i25 - i5) / fs        # T20 → T60


# --------------------------------------------------------------------------- next-door houses
# Door key characters follow the "portals" order of the pack's geometry (1 = open, 0 = closed).
NEXT_DOOR = {
    "House 1": {
        "doors": ["d1", "d2", "w"], "default": "110",
        "map": {
            "0 living close": ("s_living", "Dnevna soba (blizu)", "111"),
            "1 ground level hall door 1 open": ("s_hall", "Hol u prizemlju", "111"),
            "2 ground level hall door 1 closed": ("s_hall", "Hol u prizemlju", "011"),
            "3 guest room door 1 and 2 open": ("s_guest", "Gostinska soba", "111"),
            "4 guest room door 1 closed 2 open": ("s_guest", "Gostinska soba", "011"),
            "5 guest room door 1 and 2 closed": ("s_guest", "Gostinska soba", "001"),
            "6 study door 1 and 2 open": ("s_study", "Radna soba", "111"),
            "7 study door 1 closed 2 open": ("s_study", "Radna soba", "011"),
            "8 study door 1 and 2 closed": ("s_study", "Radna soba", "001"),
            "9 upstairs hall door 1 open": ("s_upstairs", "Gornji hol", "111"),
            "10 upstairs hall door 1 ajar": ("s_upstairs_ajar", "Gornji hol (odškrinuta vrata)", "111"),
            "11 upstairs hall door 1 closed": ("s_upstairs", "Gornji hol", "011"),
            "12 bedroom door 1 and 2 open": ("s_bedroom", "Spavaća soba", "111"),
            "13 bedroom door 1 closed 2 open": ("s_bedroom", "Spavaća soba", "011"),
            "14 bedroom door 1 and 2 closed": ("s_bedroom", "Spavaća soba", "001"),
            "15 studio door 1 and 2 open": ("s_studio", "Studio", "111"),
            "16 studio door 1 closed 2 open": ("s_studio", "Studio", "011"),
            "17 studio door 1 and 2 closed": ("s_studio", "Studio", "001"),
            "18 studio door 1 opn 2 clsd": ("s_studio", "Studio", "101"),
            "19 speaker outside": ("s_outside", "Napolju", "110"),
            "20 speaker outside door closed": ("s_outside", "Napolju", "010"),
            "21 speaker out window open": ("s_outside", "Napolju", "011"),
            "22 speaker out window 1-2 open": ("s_outside_w2", "Napolju (oba prozora otvorena)", "011"),
        },
    },
    "Coll's place": {
        "doors": ["d1"], "default": "1",
        "map": {
            "1 outside. downst. bedrm.": ("s_down_out", "Ispred donje spavaće sobe", "1"),
            "2 inside downst. bedrm. opn": ("s_down", "Donja spavaća soba", "1"),
            "3 inside downst. bedrm. clsd": ("s_down", "Donja spavaća soba", "0"),
            "4 outside upst. bedrm": ("s_up_out", "Ispred gornje spavaće sobe", "1"),
            "5 inside upst. bedrm opn": ("s_up", "Gornja spavaća soba", "1"),
            "6 inside upst. bedrm clsd": ("s_up", "Gornja spavaća soba", "0"),
            "7 spkr in small hallway open": ("s_hall", "Mali hodnik", "1"),
            "8 spkr in small hallway clsd": ("s_hall", "Mali hodnik", "0"),
        },
    },
    "Next Door III - Coll": {
        "doors": ["d1"], "default": "1",
        "map": {
            "04 mud room to family room": ("s_mud_family", "Ostava → porodična soba", "1"),
            "05 mudr. to family r. door clsd": ("s_mud_family", "Ostava → porodična soba", "0"),
            "13 bedroom to living room": ("s_bed_living", "Spavaća → dnevna", "1"),
            "14 bedr to living door clsd": ("s_bed_living", "Spavaća → dnevna", "0"),
        },
    },
}


def geometry_plain(sources):
    """one schematic room, mic at the front, sources on an arc"""
    n = len(sources)
    out = []
    for i, s in enumerate(sources):
        side = s.get("side", 0)
        base = i if side == 0 else s["base_index"]
        ang = np.pi / 2 + (base - (s["n_centres"] - 1) / 2) * (np.pi / max(3, s["n_centres"] + 1))
        r = 2.2
        x, y = 3 + r * np.cos(ang), 1.2 + r * np.sin(ang)
        x += 0.8 * side
        out.append({"id": s["id"], "name": s["name"], "room": "room",
                    "pos": [round(float(np.clip(x, 0.3, 5.7)), 2), round(float(np.clip(y, 0.3, 4.7)), 2), 1.5]})
    loc = {
        "rooms": [{"id": "room", "name": "Prostor", "rect": [0, 0, 6, 5], "height": 2.7}],
        "sources": out,
        "mics": [{"id": "m_main", "name": "Altiverb mikrofoni", "pos": [3, 1.2, 1.6], "yaw": 90, "room": "room"}],
    }
    return loc


def geometry_doors(sources, spec):
    """schematic house: source room - hall - listener room (+ outside zone for windows)"""
    rooms = [
        {"id": "izvor", "name": "Soba sa izvorom", "rect": [0, 0, 4, 5], "height": 2.6},
        {"id": "hodnik", "name": "Hodnik", "rect": [4, 0, 6, 5], "height": 2.6},
        {"id": "slusalac", "name": "Soba sa mikrofonom", "rect": [6, 0, 11, 5], "height": 2.6},
    ]
    portals = []
    names = {"d1": "Vrata 1", "d2": "Vrata 2", "w": "Prozor"}
    has_outside = "w" in spec["doors"]
    if has_outside:
        rooms.append({"id": "napolje", "name": "Napolju", "rect": [0, 5, 11, 9], "exterior": True})
    default = spec["default"]
    for i, d in enumerate(spec["doors"]):
        is_open = default[i] == "1"
        if d == "d1":
            portals.append({"id": "d1", "name": names[d], "type": "door", "rooms": ["izvor", "hodnik"], "at": [4, 2.5], "open": is_open})
        elif d == "d2":
            portals.append({"id": "d2", "name": names[d], "type": "door", "rooms": ["hodnik", "slusalac"], "at": [6, 2.5], "open": is_open})
        elif d == "w":
            portals.append({"id": "w", "name": names[d], "type": "window", "rooms": ["slusalac", "napolje"], "at": [8.5, 5], "open": is_open})
    if len(spec["doors"]) == 1:
        # single door: source room directly next to the listener room
        rooms = [
            {"id": "izvor", "name": "Soba sa izvorom", "rect": [0, 0, 4, 5], "height": 2.6},
            {"id": "slusalac", "name": "Soba sa mikrofonom", "rect": [4, 0, 9, 5], "height": 2.6},
        ]
        portals = [{"id": "d1", "name": "Vrata", "type": "door", "rooms": ["izvor", "slusalac"], "at": [4, 2.5],
                    "open": default[0] == "1"}]
    mic_x = 8.5 if len(spec["doors"]) > 1 else 6.5
    out = []
    n = len(sources)
    for i, s in enumerate(sources):
        sid = s["id"]
        if sid.startswith("s_outside"):
            pos, room = [3 + i * 0.6, 7.0, 1.5], "napolje"
        elif sid == "s_living":
            pos, room = [mic_x - 1.5, 3.5, 1.5], "slusalac"
        elif "_out" in sid and len(spec["doors"]) == 1:
            pos, room = [3.6, 0.8 + 3.4 * i / max(1, n - 1), 1.5], "izvor"
        else:
            pos, room = [0.6 + 2.8 * (i % 4) / 3, 0.8 + 3.4 * (i // 4) / max(1, (n - 1) // 4), 1.5], "izvor"
        if not has_outside and room == "napolje":
            room = "izvor"
        out.append({"id": sid, "name": s["name"], "room": room, "pos": [round(p, 2) for p in pos]})
    return {
        "rooms": rooms,
        "portals": portals,
        "sources": out,
        "mics": [{"id": "m_main", "name": "Altiverb mikrofoni", "pos": [mic_x, 2.5, 1.6], "yaw": 180, "room": "slusalac"}],
    }


# --------------------------------------------------------------------------- main
def build(a):
    rows = list(csv.DictReader(open(a.manifest, encoding="utf-8")))
    groups = {}
    for r in rows:
        groups.setdefault((r["cat"], r["room"]), []).append(r)

    os.makedirs(a.out, exist_ok=True)
    summary = []
    missing = []
    for (cat, room), items in sorted(groups.items()):
        spec = NEXT_DOOR.get(room) if cat == "next door" else None
        pack_name = f"{a.prefix}{room}"
        pack_dir = os.path.join(a.out, lua_sanitize(pack_name).replace("_", " ").strip())
        ir_dir = os.path.join(pack_dir, "ir")
        if os.path.isdir(pack_dir):
            shutil.rmtree(pack_dir)
        os.makedirs(ir_dir)

        sources = []          # dicts: id, name (+ geometry helpers)
        seen_ids = set()
        rts = []
        n_centres = len(items)
        for k, r in enumerate(sorted(items, key=lambda r: r["pos"])):
            base = "__".join(lua_sanitize(v) for v in (r["cat"], r["room"], r["pos"]))
            f = {s: os.path.join(a.captures, f"{base}__{s}.wav") for s in "CLR"}
            have = {s: os.path.isfile(p) for s, p in f.items()}
            if spec is not None:
                sid, sname, key = spec["map"].get(r["pos"], ("s_" + slug(r["pos"]), r["pos"], "1" * len(spec["doors"])))
            else:
                sid, sname, key = "s_" + slug(r["pos"]), (r["pos"] if r["pos"] != "main" else "Glavna pozicija"), "-"

            variants = []
            if have["L"] and have["R"]:
                xl, fs = load_capture(f["L"]); xr, _ = load_capture(f["R"])
                xc = load_capture(f["C"])[0] if have["C"] else 0.5 * (xl + xr)
                variants = [(sid, sname, xc, 0), (sid + "_l", sname + " · levo", xl, -1), (sid + "_r", sname + " · desno", xr, +1)]
            elif have["C"]:
                xc, fs = load_capture(f["C"])
                variants = [(sid, sname, xc, 0)]
            else:
                missing.append(base)
                continue

            for vid, vname, x, side in variants:
                y = trim(x, fs)
                rts.append(rt_estimate(y, fs))
                sf.write(os.path.join(ir_dir, f"{vid}__m_main__{key}.wav"), y.astype(np.float32), fs, subtype="FLOAT")
                if vid not in seen_ids:
                    seen_ids.add(vid)
                    sources.append({"id": vid, "name": vname, "side": side, "base_index": k, "n_centres": n_centres})

        if not sources:
            shutil.rmtree(pack_dir)
            continue

        geo = geometry_doors(sources, spec) if spec is not None else geometry_plain(sources)
        centre_ids = [s["id"] for s in sources if s["side"] == 0]
        cat_name = CATEGORY_NAMES.get(cat, cat)
        rt = float(np.median(rts)) if rts else 0.0
        loc = {
            "format": "subroo-location", "version": 1,
            "id": "av_" + slug(cat) + "_" + slug(room),
            "name": room,
            "category": f"Altiverb · {cat_name}",
            "description": f"Altiverb 7 XL · POST / {cat} · {len(sources)} pozicija · RT ≈ {rt:.2f} s. "
                           "Snimljeno kroz Altiverb (impuls, 100 % wet, direct uključen). Geometrija je šematska.",
            **geo,
            "defaults": {"source": centre_ids[0] if centre_ids else sources[0]["id"], "mic": "m_main"},
            "pack": {"rig": RIG_STEREO, "calibrationDb": a.calibration, "irFolder": "ir",
                     "origin": "Audio Ease Altiverb 7 XL (captured with permission)"},
        }
        with open(os.path.join(pack_dir, "location.json"), "w", encoding="utf-8") as fh:
            json.dump(loc, fh, ensure_ascii=False, indent=1)
        summary.append((cat, room, len(sources), rt))
        print(f"  {cat:24s} {room:45s} {len(sources):3d} src  RT {rt:4.2f} s")

    print(f"{len(summary)} packs -> {a.out}")
    if missing:
        print(f"{len(missing)} captures missing, e.g. {missing[:3]}")
    return summary, missing


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--captures", required=True)
    p.add_argument("--manifest", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--prefix", default="AV - ")
    p.add_argument("--calibration", type=float, default=0.0)
    a = p.parse_args()
    build(a)


if __name__ == "__main__":
    main()
