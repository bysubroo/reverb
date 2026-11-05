#!/usr/bin/env python3
"""Turn the Altiverb IR catalog (hashfloat;hash;path) into capture_list.csv + manifest."""
import csv, html, re, sys, collections

COLLECTIONS = {"Tight spots", "Castle of Zuylen", "Streets", "Underground", "Forests",
               "Alleys", "Tunnels", "Fields"}
FMT = re.compile(r'\b([msq])-([msq])\b')

def parse(path):
    p = html.unescape(path)
    seg = p.split('/')
    if seg[-1] == 'info.iri2':
        seg = seg[:-1]
    bank = seg[1]               # POST / MUSIC
    cat = seg[2]
    rest = seg[3:]
    last = rest[-1]
    m = FMT.search(last)
    fmt = m.group(0) if m else 'm-s'
    remainder = FMT.sub('', last).strip(' -')
    if remainder and len(rest) >= 2:
        group, pos = rest[:-1], remainder
    elif remainder:
        group, pos = [remainder], 'main'
    elif len(rest) >= 3:
        group, pos = rest[:-2], rest[-2]
    elif len(rest) == 2:
        group, pos = [rest[0]], 'main'
    else:
        group, pos = [last], 'main'
    if len(group) == 1 and group[0] in COLLECTIONS:
        group, pos = [group[0], pos], 'main'
    return bank, cat, ' - '.join(g.strip() for g in group), pos.strip(), fmt, '/'.join(seg)

def mode_for(fmt):
    return 'ss' if fmt == 's-s' else 'ms'

def main(catalog, out_list, out_manifest):
    rows = []
    for line in open(catalog, encoding='utf-8'):
        line = line.rstrip('\n')
        if not line:
            continue
        hf, h, path = line.split(';', 2)
        bank, cat, room, pos, fmt, clean = parse(path)
        rows.append(dict(hf=hf, hash=h, bank=bank, cat=cat, room=room, pos=pos, fmt=fmt,
                         mode=mode_for(fmt), path=clean))
    # calibration rows first
    for r in rows:
        if r['path'].endswith('domestic/Bathroom 1/m-s'):
            r['mode'] = 'cal'
        if r['path'].endswith('domestic/Bedroom 1/s-s'):
            r['mode'] = 'cal'
    rows.sort(key=lambda r: (r['mode'] != 'cal', r['cat'], r['room'], r['pos']))
    names = collections.Counter((r['cat'], r['room'], r['pos']) for r in rows)
    for r in rows:
        if names[(r['cat'], r['room'], r['pos'])] > 1:
            r['pos'] = f"{r['pos']} {r['fmt']}"
    names = collections.Counter((r['cat'], r['room'], r['pos']) for r in rows)
    dup = [k for k, v in names.items() if v > 1]
    assert not dup, dup
    with open(out_list, 'w', encoding='utf-8', newline='\n') as f:
        for r in rows:
            f.write(f"{r['hf']};{r['cat']};{r['room']};{r['pos']};{r['mode']}\n")
    with open(out_manifest, 'w', encoding='utf-8', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    renders = sum(3 if r['mode'] == 'cal' else 2 if r['mode'] == 'ss' else 1 for r in rows)
    print(len(rows), 'IRs,', renders, 'renders')

if __name__ == '__main__':
    main(*sys.argv[1:4])
