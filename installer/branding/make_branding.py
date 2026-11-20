"""Generates the installer / app icon artwork (original SubRoo Rooms design).
Usage: python3 make_branding.py <repo root>"""
import sys, os, math, random
from PIL import Image, ImageDraw, ImageFont, ImageFilter

root = sys.argv[1] if len(sys.argv) > 1 else '.'
fonts = os.path.join(root, 'Resources', 'fonts')
out_win = os.path.join(root, 'installer', 'windows')
out_icon = os.path.join(root, 'Resources', 'icon')
os.makedirs(out_win, exist_ok=True); os.makedirs(out_icon, exist_ok=True)

PINK = (254, 32, 215); CYAN = (46, 242, 255); AMBER = (255, 181, 74)
SILK = (236, 230, 214); SILKDIM = (168, 162, 148)

def font(weight, size):
    return ImageFont.truetype(os.path.join(fonts, f'BarlowCondensed-{weight}.ttf'), size)

def brushed(w, h, top=(60, 62, 67), bottom=(43, 44, 48), seed=7):
    rnd = random.Random(seed)
    img = Image.new('RGB', (w, h)); px = img.load()
    drift = 0.0
    for y in range(h):
        drift = drift * 0.82 + (rnd.random() - 0.5) * 0.9
        t = y / max(1, h - 1)
        base = [top[i] + (bottom[i] - top[i]) * t for i in range(3)]
        streak, left = 0.0, 0
        for x in range(w):
            if left <= 0:
                left = 20 + rnd.randrange(220); streak = (rnd.random() - 0.5) * 5
            left -= 1
            n = drift * 3 + streak + (rnd.random() - 0.5) * 3
            px[x, y] = tuple(max(0, min(255, int(base[i] + n))) for i in range(3))
    return img

def iso_room(draw, cx, cy, s, width=2):
    """Isometric room outline: floor diamond + walls, a source (pink) and a mic (cyan)."""
    def P(x, y, z):  # room coords (0..1) -> screen
        return (cx + (x - y) * s * 0.87, cy + (x + y) * s * 0.5 - z * s * 0.9)
    floor = [P(0, 0, 0), P(1, 0, 0), P(1, 1, 0), P(0, 1, 0)]
    back = [P(0, 0, 0), P(0, 0, 0.55), P(1, 0, 0.55), P(1, 0, 0)]
    left = [P(0, 0, 0), P(0, 0, 0.55), P(0, 1, 0.55), P(0, 1, 0)]
    draw.polygon(floor, fill=(30, 36, 44))
    draw.polygon(back, fill=(52, 58, 66))
    draw.polygon(left, fill=(42, 47, 54))
    for poly in (floor, back, left):
        draw.line(poly + [poly[0]], fill=(150, 156, 165), width=width)
    # inner wall with door gap
    a, b = P(0.55, 0, 0), P(0.55, 0.35, 0)
    draw.line([a, b], fill=(150, 156, 165), width=width)
    c, d = P(0.55, 0.62, 0), P(0.55, 1, 0)
    draw.line([c, d], fill=(150, 156, 165), width=width)
    sx, sy = P(0.28, 0.5, 0.0); r = s * 0.075
    draw.rectangle([sx - r, sy - r, sx + r, sy + r], fill=PINK)
    mx, my = P(0.78, 0.55, 0.0); r2 = s * 0.07
    draw.ellipse([mx - r2, my - r2 * 1.2, mx + r2, my + r2 * 1.2], fill=CYAN)
    # sound path
    draw.line([(sx, sy), (mx, my)], fill=AMBER, width=max(1, width - 1))

def rounded_mask(size, radius):
    m = Image.new('L', (size, size), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    return m

# ------------------------------------------------------------- app icon (1024)
def make_icon(size=1024):
    base = brushed(size, size, (66, 68, 74), (36, 37, 41), seed=11)
    d = ImageDraw.Draw(base)
    # bezel ring
    pad = int(size * 0.06)
    d.rounded_rectangle([pad, pad, size - pad, size - pad], radius=int(size * 0.16), outline=(18, 19, 21), width=int(size * 0.02))
    # recessed screen
    sp = int(size * 0.14)
    d.rounded_rectangle([sp, sp, size - sp, int(size * 0.74)], radius=int(size * 0.06), fill=(10, 13, 16), outline=(0, 0, 0), width=6)
    iso_room(d, size * 0.5, size * 0.33, size * 0.32, width=max(2, size // 170))
    # wordmark
    f = font('Bold', int(size * 0.15))
    t1, t2 = 'SUBROO', 'ROOMS'
    w1 = d.textlength(t1, font=f); w2 = d.textlength(t2, font=f)
    gap = size * 0.03; x = (size - (w1 + gap + w2)) / 2; y = size * 0.755
    d.text((x, y), t1, font=f, fill=PINK)
    d.text((x + w1 + gap, y), t2, font=f, fill=SILK)
    img = base.convert('RGBA')
    img.putalpha(rounded_mask(size, int(size * 0.2)))
    return img

icon = make_icon(1024)
icon.save(os.path.join(out_icon, 'icon_1024.png'))
icon.save(os.path.join(out_win, 'SubRooRooms.ico'), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])

# ------------------------------------------------------------- wizard images
def wizard_large(w, h):
    img = brushed(w, h, (58, 60, 65), (32, 33, 37), seed=3)
    d = ImageDraw.Draw(img)
    k = w / 164.0
    # rack ear strip on the left
    d.rectangle([0, 0, int(10 * k), h], fill=(80, 82, 87))
    d.line([(int(10 * k), 0), (int(10 * k), h)], fill=(10, 10, 12), width=max(1, int(2 * k)))
    for yy in (0.06, 0.94):
        cy = h * yy; r = 3.2 * k
        d.ellipse([5 * k - r, cy - r, 5 * k + r, cy + r], fill=(200, 200, 198), outline=(30, 30, 32))
    # screen with the room
    sx0, sy0, sx1, sy1 = 20 * k, 24 * k, w - 10 * k, 150 * k
    d.rounded_rectangle([sx0 - 4 * k, sy0 - 4 * k, sx1 + 4 * k, sy1 + 4 * k], radius=int(7 * k), fill=(70, 72, 77))
    d.rounded_rectangle([sx0, sy0, sx1, sy1], radius=int(4 * k), fill=(10, 13, 16))
    iso_room(d, (sx0 + sx1) / 2, sy0 + 26 * k, 54 * k, width=max(1, int(1.6 * k)))
    # wordmark stacked
    f = font('Bold', int(34 * k)); f2 = font('SemiBold', int(10.5 * k))
    d.text((20 * k, 170 * k), 'SUBROO', font=f, fill=PINK)
    d.text((20 * k, 204 * k), 'ROOMS', font=f, fill=SILK)
    d.text((21 * k, 246 * k), 'LOCATION REVERB', font=f2, fill=SILKDIM)
    d.text((21 * k, 260 * k), 'MEASURED SPACES', font=f2, fill=SILKDIM)
    # lamp
    lr = 3.6 * k; lx, ly = 26 * k, 292 * k
    d.ellipse([lx - lr * 1.5, ly - lr * 1.5, lx + lr * 1.5, ly + lr * 1.5], fill=(190, 190, 190))
    d.ellipse([lx - lr, ly - lr, lx + lr, ly + lr], fill=AMBER)
    d.text((lx + 9 * k, ly - 7 * k), 'POWER', font=font('Bold', int(10 * k)), fill=SILKDIM)
    return img

for scale, tag in ((1.0, '100'), (1.5, '150'), (2.0, '200')):
    w, h = int(round(164 * scale)), int(round(314 * scale))
    wizard_large(w, h).save(os.path.join(out_win, f'wizard-large-{tag}.bmp'))
    s = int(round(55 * scale))
    small = Image.new('RGB', (s, s), (255, 255, 255))
    ic = icon.resize((s, s), Image.LANCZOS)
    small.paste(ic, (0, 0), ic)
    small.save(os.path.join(out_win, f'wizard-small-{tag}.bmp'))
print('branding written')
