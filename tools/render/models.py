"""Parametric models: car interior cutaway, studio speaker, microphone."""
import bpy, math
from mathutils import Vector
from srlib import box, cyl, torus, sphere, subsurf, mat, assign, link

def _car_body(name, length, width, z0, z1, nose, tail, m, offset_x=0.0, exponent=4.5, rings=7, tumble=0.06):
    """Rounded car-like solid: superellipse outline in plan, narrowing towards the top."""
    import bmesh
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    n = 96
    a, bw = length / 2, width / 2
    layers = []
    for r in range(rings):
        t = r / (rings - 1)
        z = z0 + (z1 - z0) * t
        shrink = 1.0 - tumble * (max(0.0, t - 0.5) * 2) ** 2      # sides lean in near the top
        bottom = 1.0 - 0.05 * (max(0.0, 0.25 - t) * 4) ** 2       # and a little at the sill
        ring = []
        for i in range(n):
            th = 2 * math.pi * i / n
            c, s_ = math.cos(th), math.sin(th)
            x = a * math.copysign(abs(c) ** (2 / exponent), c)
            y = bw * math.copysign(abs(s_) ** (2 / exponent), s_)
            taper = nose if x > 0 else tail
            y *= 1.0 - (1.0 - taper) * (abs(x) / a) ** 3
            ring.append(bm.verts.new((x * min(1.0, shrink + 0.04) + offset_x, y * shrink * bottom, z)))
        layers.append(ring)
    for r in range(rings - 1):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((layers[r][i], layers[r][j], layers[r + 1][j], layers[r + 1][i]))
    bm.faces.new(list(reversed(layers[0]))); bm.faces.new(layers[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update(); bm.to_mesh(me); bm.free()
    for f in me.polygons: f.use_smooth = True
    o = link(bpy.data.objects.new(name, me))
    if m: assign(o, m)
    return o

def _bool(target, cutter, op='DIFFERENCE'):
    m = target.modifiers.new("cut", 'BOOLEAN'); m.object = cutter; m.operation = op; m.solver = 'EXACT'
    cutter.hide_render = True; cutter.hide_viewport = True
    return target

# ----------------------------------------------------------------------------- car
def car(spec):
    L, W, H = spec["length"], spec["width"], spec.get("height", 1.45)
    cut = spec.get("cutHeight", 0.98)            # body is cut away above this (roof removed)
    paint = mat("paint", tuple(spec.get("paint", (0.55, 0.06, 0.05))), rough=0.25, coat=1.0)
    trim = mat("trim", (0.05, 0.05, 0.055), rough=0.55, kind="plastic", scale=1)
    dash = mat("dash", (0.07, 0.07, 0.075), rough=0.6, kind="plastic", scale=1)
    fabric = mat("seat", tuple(spec.get("seat", (0.14, 0.14, 0.16))), rough=0.9, kind="fabric", scale=2)
    carpet = mat("carpet", (0.06, 0.06, 0.065), rough=1.0, kind="carpet", scale=1)
    rubber = mat("rubber", (0.02, 0.02, 0.02), rough=0.8)
    alloy = mat("alloy", (0.75, 0.76, 0.78), rough=0.25, metal=1.0)
    chrome = mat("chrome", (0.9, 0.9, 0.9), rough=0.08, metal=1.0)
    glass = mat("glass", (0.6, 0.7, 0.75), rough=0.02, trans=1.0)

    fx = L / 2           # front bumper x
    rx = -L / 2          # rear bumper x
    dashX = spec.get("dashX", fx - 1.30)      # back face of dashboard
    trunkX = spec.get("trunkX", rx + 0.75)    # rear seat back / trunk front

    # body: superellipse plan outline lofted upwards (tumblehome), wheel arches and the cabin cut out
    body = _car_body("body", L, W, 0.24, cut, spec.get("noseTaper", 0.86), spec.get("tailTaper", 0.93), paint)
    cab = _car_body("cabin_cut", (dashX - rx - 0.1), W - 0.16, 0.34, cut + 0.5, 1.0, 1.0, None,
                    offset_x=(dashX + rx + 0.1) / 2, exponent=8)
    _bool(body, cab)
    wb = spec.get("wheelbase", 2.58)
    for i, x in enumerate((wb / 2 + 0.05, -wb / 2 + 0.05)):
        arch = cyl(f"arch{i}", (x, 0, 0.31), 0.37, W + 0.4, None, axis='Y', verts=64)
        _bool(body, arch)
    b = body.modifiers.new("bevel", 'BEVEL'); b.width = 0.012; b.segments = 3; b.limit_method = 'ANGLE'
    body.modifiers.new("wn", 'WEIGHTED_NORMAL')
    # hood: sloped cover in front of the windshield line
    # wheels
    for i, (x, y) in enumerate(((wb / 2 + 0.05, W / 2 - 0.12), (wb / 2 + 0.05, -W / 2 + 0.12),
                                (-wb / 2 + 0.05, W / 2 - 0.12), (-wb / 2 + 0.05, -W / 2 + 0.12))):
        t = cyl(f"tire{i}", (x, y, 0.31), 0.31, 0.2, rubber, axis='Y', bevel=0.04)
        cyl(f"rim{i}", (x, y + (0.101 if y > 0 else -0.101), 0.31), 0.2, 0.01, alloy, axis='Y')
    # windshield frame stub (base of the glass)
    box("cowl", (dashX + 0.32, 0, cut + 0.02), (0.08, W - 0.18, 0.04), trim)
    # floor + carpet
    box("floor", ((dashX + trunkX) / 2, 0, 0.36), (dashX - trunkX, W - 0.2, 0.04), carpet, bevel=0.01)
    box("tunnel", ((dashX - 0.9 + dashX) / 2, 0, 0.45), (0.9, 0.22, 0.16), carpet, bevel=0.05)
    # dashboard
    d = box("dashboard", (dashX + 0.22, 0, cut - 0.18), (0.44, W - 0.16, 0.36), dash, bevel=0.06, segments=5)
    box("dash_top", (dashX + 0.30, 0, cut - 0.01), (0.30, W - 0.2, 0.04), dash, bevel=0.02)
    box("cluster_hood", (dashX + 0.12, spec.get("driverY", 0.37), cut + 0.03), (0.16, 0.36, 0.1), dash, bevel=0.04)
    for k in (-1, 1):
        cyl(f"vent{k}", (dashX - 0.005, 0.62 * k, cut - 0.16), 0.045, 0.02, chrome, axis='X')
    box("center_stack", (dashX + 0.05, 0, cut - 0.30), (0.16, 0.28, 0.36), trim, bevel=0.03)
    box("screen", (dashX - 0.035, 0, cut - 0.18), (0.01, 0.2, 0.12), mat("screen", (0.02, 0.03, 0.05), rough=0.1))
    box("console", (dashX - 0.45, 0, 0.53), (0.7, 0.2, 0.12), trim, bevel=0.03)
    cyl("shifter", (dashX - 0.25, 0, 0.66), 0.012, 0.16, chrome)
    sphere("shift_knob", (dashX - 0.25, 0, 0.75), 0.03, trim)
    # steering wheel
    dy = spec.get("driverY", 0.37)
    sw_c = Vector((dashX - 0.28, dy, cut - 0.06))
    torus("steering", sw_c, 0.18, 0.018, rubber, rot=(0, math.radians(68), 0))
    cyl("column", sw_c + Vector((0.15, 0, -0.06)), 0.03, 0.32, trim, rot=(0, math.radians(68 + 90), 0))
    box("sw_hub", sw_c, (0.05, 0.12, 0.08), trim, bevel=0.02, rot=(0, math.radians(-22), 0))
    # door trims (inside of the tub)
    for k in (-1, 1):
        x0, x1 = trunkX + 0.15, dashX - 0.05
        box(f"doortrim{k}", ((x0 + x1) / 2, k * (W / 2 - 0.095), 0.68), (x1 - x0, 0.02, 0.48), trim, bevel=0.01)
        box(f"armrest{k}", (dashX - 0.65, k * (W / 2 - 0.12), 0.7), (0.4, 0.06, 0.05), fabric, bevel=0.02)
    # seats
    def seat(name, x, y, w=0.5, bench=False):
        cw = w if not bench else w
        c = subsurf(box(name + "_cushion", (x, y, 0.52), (0.5, cw, 0.14), fabric, bevel=0.02, segments=2), 2)
        bk = subsurf(box(name + "_back", (x - 0.27, y, 0.86), (0.13, cw, 0.62), fabric, bevel=0.02, segments=2), 2)
        bk.rotation_euler = (0, math.radians(-12), 0)
        if not bench:
            box(name + "_bolsterL", (x - 0.02, y + w / 2 - 0.04, 0.6), (0.48, 0.07, 0.1), fabric, bevel=0.03)
            box(name + "_bolsterR", (x - 0.02, y - w / 2 + 0.04, 0.6), (0.48, 0.07, 0.1), fabric, bevel=0.03)
            box(name + "_head", (x - 0.36, y, 1.25), (0.1, 0.26, 0.18), fabric, bevel=0.04)
            cyl(name + "_post", (x - 0.34, y, 1.15), 0.008, 0.12, chrome)
        return c
    frontX = dashX - 0.85
    seat("seat_driver", frontX, dy)
    seat("seat_pass", frontX, -dy)
    rearX = trunkX + 0.5
    seat("bench", rearX, 0.0, w=W - 0.3, bench=True)
    for k in (-1, 1):
        box(f"rear_head{k}", (rearX - 0.37, k * 0.38, 1.2), (0.09, 0.24, 0.15), fabric, bevel=0.04)
    # trunk
    box("trunk_floor", ((trunkX + rx + 0.4) / 2, 0, 0.5), (trunkX - rx - 0.45, W - 0.62, 0.03), carpet)
    # lights (front / rear)
    for k in (-1, 1):
        box(f"headlight{k}", (fx - 0.1, k * (W / 2 - 0.32), cut - 0.12), (0.06, 0.3, 0.1), mat("lamp", (0.85, 0.87, 0.9), rough=0.05, metal=0.6), bevel=0.03)
        box(f"taillight{k}", (rx + 0.07, k * (W / 2 - 0.3), cut - 0.12), (0.04, 0.26, 0.12), mat("tail", (0.6, 0.02, 0.02), rough=0.2), bevel=0.02)
    # mirrors
    for k in (-1, 1):
        box(f"mirror{k}", (dashX + 0.15, k * (W / 2 + 0.08), cut + 0.03), (0.1, 0.16, 0.1), paint, bevel=0.03)
    return {"center": (0, 0, 0.6), "extent": max(L, W) * 1.25}

# ----------------------------------------------------------------------------- markers
def speaker(loc=(0, 0, 0), yaw=0.0, scale=1.0):
    """Compact two-way monitor, front faces +x before rotation."""
    s = scale
    shell = mat("spk_shell", (0.86, 0.86, 0.87), rough=0.35)
    cone = mat("spk_cone", (0.03, 0.03, 0.03), rough=0.7, kind="plastic", scale=4)
    ring = mat("spk_ring", (0.5, 0.5, 0.52), rough=0.3, metal=1.0)
    accent = mat("spk_accent", (0.996, 0.125, 0.843), rough=0.4)
    parts = []
    parts.append(box("spk_box", (0, 0, 0.16 * s), (0.2 * s, 0.16 * s, 0.30 * s), shell, bevel=0.015 * s, segments=4))
    parts.append(cyl("spk_woofer_ring", (0.101 * s, 0, 0.12 * s), 0.062 * s, 0.006 * s, ring, axis='X'))
    parts.append(cyl("spk_woofer", (0.104 * s, 0, 0.12 * s), 0.055 * s, 0.004 * s, cone, axis='X'))
    parts.append(sphere("spk_dust", (0.104 * s, 0, 0.12 * s), 0.018 * s, cone, scale=(0.5, 1, 1)))
    parts.append(cyl("spk_tw_ring", (0.101 * s, 0, 0.245 * s), 0.025 * s, 0.006 * s, ring, axis='X'))
    parts.append(sphere("spk_tweeter", (0.104 * s, 0, 0.245 * s), 0.014 * s, ring, scale=(0.6, 1, 1)))
    parts.append(box("spk_led", (0.101 * s, 0, 0.285 * s), (0.004 * s, 0.03 * s, 0.006 * s), accent))
    parts.append(box("spk_top", (0, 0, 0.311 * s), (0.17 * s, 0.13 * s, 0.004 * s), accent, bevel=0.002 * s))
    return _group("speaker", parts, loc, yaw)

def microphone(loc=(0, 0, 0), yaw=0.0, scale=1.0):
    """Large-diaphragm condenser in a shock mount on a short stand, front faces +x."""
    s = scale
    body = mat("mic_body", (0.72, 0.73, 0.75), rough=0.22, metal=1.0)
    grille = mat("mic_grille", (0.6, 0.6, 0.62), rough=0.35, metal=1.0, kind="fabric", scale=30)
    dark = mat("mic_dark", (0.10, 0.10, 0.11), rough=0.5)
    accent = mat("mic_accent", (0.18, 0.95, 1.0), rough=0.3)
    parts = []
    parts.append(cyl("mic_stand", (0, 0, 0.11 * s), 0.008 * s, 0.22 * s, dark))
    parts.append(cyl("mic_base", (0, 0, 0.004 * s), 0.07 * s, 0.008 * s, dark, bevel=0.003 * s))
    parts.append(torus("mic_shock", (0, 0, 0.31 * s), 0.05 * s, 0.006 * s, dark))
    parts.append(cyl("mic_tube", (0, 0, 0.29 * s), 0.028 * s, 0.13 * s, body))
    parts.append(sphere("mic_head", (0, 0, 0.385 * s), 0.034 * s, grille, scale=(1, 1, 1.35)))
    parts.append(box("mic_badge", (0.028 * s, 0, 0.30 * s), (0.004 * s, 0.012 * s, 0.02 * s), accent))
    parts.append(torus("mic_ring", (0, 0, 0.352 * s), 0.029 * s, 0.004 * s, accent))
    return _group("microphone", parts, loc, yaw)

def _group(name, parts, loc, yaw):
    root = link(bpy.data.objects.new(name, None))
    for p in parts:
        p.parent = root
    root.location = loc; root.rotation_euler = (0, 0, yaw)
    return root
