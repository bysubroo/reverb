"""Builds the AES-256 encrypted 7z pack archive used by the installers.
usage: python3 make_pack_archive.py <packs folder> <out .7z> <key file>   (pip install py7zr)"""
import py7zr, os, sys, hashlib, time
src, out, keyfile = sys.argv[1], sys.argv[2], sys.argv[3]
key = open(keyfile).read().strip()
filters = [{'id': py7zr.FILTER_LZMA2, 'preset': 7}, {'id': py7zr.FILTER_CRYPTO_AES256_SHA256}]
t = time.time(); total = 0; n = 0
with py7zr.SevenZipFile(out, 'w', password=key, header_encryption=True, filters=filters) as z:
    for d in sorted(os.listdir(src)):
        p = os.path.join(src, d)
        if not os.path.isdir(p): continue
        for root, _, files in os.walk(p):
            for f in sorted(files):
                fp = os.path.join(root, f)
                z.write(fp, os.path.relpath(fp, src))
                total += os.path.getsize(fp); n += 1
h = hashlib.sha256(open(out, 'rb').read()).hexdigest()
print(f"files={n} uncompressed={total} archive={os.path.getsize(out)} sha256={h} secs={time.time()-t:.0f}")
