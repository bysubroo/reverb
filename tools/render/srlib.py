"""SubRoo Rooms render library (Blender / bpy): materials, primitives, camera, lights.
All geometry and materials are procedural and original (no external assets)."""
import bpy, bmesh, math
from mathutils import Vector

# ----------------------------------------------------------------------------- scene
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 48
    sc.cycles.use_denoising = True
    sc.cycles.max_bounces = 6
    sc.render.film_transparent = True
    sc.cycles.film_transparent_glass = True
    sc.view_settings.view_transform = 'AgX'
    sc.view_settings.look = 'AgX - Medium High Contrast'
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGBA'
    world = bpy.data.worlds.new("World"); sc.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.78, 0.80, 0.84, 1); bg.inputs[1].default_value = 0.55
    return sc

def link(obj):
    bpy.context.scene.collection.objects.link(obj); return obj

# ----------------------------------------------------------------------------- materials
_mats = {}
def mat(name, base=(0.8, 0.8, 0.8), rough=0.5, metal=0.0, kind=None, scale=8.0, var=0.15, coat=0.0, alpha=1.0, trans=0.0):
    key = (name, base, rough, metal, kind, scale, var, coat, alpha, trans)
    if key in _mats: return _mats[key]
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; N = nt.nodes; L = nt.links
    p = N["Principled BSDF"]
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metal
    p.inputs["Coat Weight"].default_value = coat
    if trans > 0:
        p.inputs["Transmission Weight"].default_value = trans
    if alpha < 1.0:
        p.inputs["Alpha"].default_value = alpha
    tc = N.new("ShaderNodeTexCoord"); mp = N.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (scale, scale, scale)
    L.new(tc.outputs["Object"], mp.inputs["Vector"])
    ramp_in = None
    if kind == "fabric":          # woven seat fabric: fine wave + noise
        w = N.new("ShaderNodeTexWave"); w.inputs["Scale"].default_value = 40; w.inputs["Distortion"].default_value = 2
        n = N.new("ShaderNodeTexNoise"); n.inputs["Scale"].default_value = 60
        mx = N.new("ShaderNodeMix"); mx.data_type = 'FLOAT'; mx.inputs[0].default_value = 0.5
        L.new(mp.outputs[0], w.inputs["Vector"]); L.new(mp.outputs[0], n.inputs["Vector"])
        L.new(w.outputs["Fac"], mx.inputs[2]); L.new(n.outputs["Fac"], mx.inputs[3]); ramp_in = mx.outputs[0]
        bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.35
        L.new(mx.outputs[0], bump.inputs["Height"]); L.new(bump.outputs[0], p.inputs["Normal"])
    elif kind == "wood":           # planks with grain
        w = N.new("ShaderNodeTexWave"); w.wave_type = 'BANDS'; w.inputs["Scale"].default_value = 2.5; w.inputs["Distortion"].default_value = 6
        L.new(mp.outputs[0], w.inputs["Vector"]); ramp_in = w.outputs["Fac"]
        bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.08
        L.new(w.outputs["Fac"], bump.inputs["Height"]); L.new(bump.outputs[0], p.inputs["Normal"])
    elif kind == "plastic":        # grained dashboard plastic
        n = N.new("ShaderNodeTexNoise"); n.inputs["Scale"].default_value = 300; n.inputs["Detail"].default_value = 4
        L.new(mp.outputs[0], n.inputs["Vector"]); ramp_in = n.outputs["Fac"]
        bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.15
        L.new(n.outputs["Fac"], bump.inputs["Height"]); L.new(bump.outputs[0], p.inputs["Normal"])
    elif kind == "carpet":
        n = N.new("ShaderNodeTexNoise"); n.inputs["Scale"].default_value = 400; n.inputs["Detail"].default_value = 8
        L.new(mp.outputs[0], n.inputs["Vector"]); ramp_in = n.outputs["Fac"]
        bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.5
        L.new(n.outputs["Fac"], bump.inputs["Height"]); L.new(bump.outputs[0], p.inputs["Normal"])
    elif kind == "tiles":
        b = N.new("ShaderNodeTexBrick"); b.inputs["Scale"].default_value = 1.0
        b.inputs["Mortar Size"].default_value = 0.01; b.offset = 0.0
        b.inputs["Color1"].default_value = (*base, 1); b.inputs["Color2"].default_value = (*[c * 0.94 for c in base], 1)
        b.inputs["Mortar"].default_value = (0.55, 0.55, 0.53, 1)
        L.new(mp.outputs[0], b.inputs["Vector"])
        L.new(b.outputs["Color"], p.inputs["Base Color"])
        bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.3; bump.invert = True
        L.new(b.outputs["Fac"], bump.inputs["Height"]); L.new(bump.outputs[0], p.inputs["Normal"])
        _mats[key] = m; return m
    elif kind == "plaster":
        n = N.new("ShaderNodeTexNoise"); n.inputs["Scale"].default_value = 30; n.inputs["Detail"].default_value = 10
        L.new(mp.outputs[0], n.inputs["Vector"]); ramp_in = n.outputs["Fac"]
        bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.05
        L.new(n.outputs["Fac"], bump.inputs["Height"]); L.new(bump.outputs[0], p.inputs["Normal"])
    if ramp_in is not None:
        cr = N.new("ShaderNodeValToRGB")
        cr.color_ramp.elements[0].color = (*[c * (1 - var) for c in base], 1)
        cr.color_ramp.elements[1].color = (*[min(1, c * (1 + var)) for c in base], 1)
        L.new(ramp_in, cr.inputs[0]); L.new(cr.outputs[0], p.inputs["Base Color"])
    else:
        p.inputs["Base Color"].default_value = (*base, 1)
    _mats[key] = m
    return m

def assign(obj, m):
    obj.data.materials.clear(); obj.data.materials.append(m); return obj

# ----------------------------------------------------------------------------- primitives
def box(name, center, size, m=None, bevel=0.0, segments=3, rot=(0, 0, 0)):
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    bm.to_mesh(me); bm.free()
    o = link(bpy.data.objects.new(name, me)); o.location = center; o.rotation_euler = rot
    if bevel > 0:
        b = o.modifiers.new("bevel", 'BEVEL'); b.width = bevel; b.segments = segments; b.limit_method = 'NONE'
    for f in me.polygons: f.use_smooth = True
    if m: assign(o, m)
    return o

def cyl(name, center, r, h, m=None, axis='Z', verts=48, bevel=0.0, rot=None):
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=r, radius2=r, depth=h)
    bm.to_mesh(me); bm.free()
    o = link(bpy.data.objects.new(name, me)); o.location = center
    if rot is not None: o.rotation_euler = rot
    elif axis == 'X': o.rotation_euler = (0, math.pi / 2, 0)
    elif axis == 'Y': o.rotation_euler = (math.pi / 2, 0, 0)
    if bevel > 0:
        b = o.modifiers.new("bevel", 'BEVEL'); b.width = bevel; b.segments = 3; b.limit_method = 'ANGLE'
    sm = o.modifiers.new("ws", 'WEIGHTED_NORMAL') if False else None
    for f in me.polygons: f.use_smooth = True
    if m: assign(o, m)
    return o

def torus(name, center, R, r, m=None, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=r, major_segments=64, minor_segments=16,
                                     location=center, rotation=rot)
    o = bpy.context.active_object; o.name = name
    bpy.ops.object.shade_smooth()
    if m: assign(o, m)
    return o

def sphere(name, center, r, m=None, scale=(1, 1, 1)):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=center, segments=48, ring_count=24)
    o = bpy.context.active_object; o.name = name; o.scale = scale
    bpy.ops.object.shade_smooth()
    if m: assign(o, m)
    return o

def subsurf(o, levels=2):
    s = o.modifiers.new("subsurf", 'SUBSURF'); s.levels = levels; s.render_levels = levels; return o

# ----------------------------------------------------------------------------- camera / light
def iso_camera(target, extent, azimuth_deg=45.0, elevation_deg=38.0, aspect=1.6):
    cam = bpy.data.cameras.new("cam"); cam.type = 'ORTHO'
    o = link(bpy.data.objects.new("cam", cam))
    az, el = math.radians(azimuth_deg), math.radians(elevation_deg)
    d = 30.0
    o.location = Vector(target) + Vector((math.cos(az) * math.cos(el), math.sin(az) * math.cos(el), math.sin(el))) * d
    look = (Vector(target) - o.location).to_track_quat('-Z', 'Y'); o.rotation_euler = look.to_euler()
    cam.ortho_scale = extent
    cam.clip_end = 100
    bpy.context.scene.camera = o
    return o

def lights(target, size=8.0, strength=1.0):
    t = Vector(target)
    for name, off, power, sz in (("key", (-4, -6, 9), 900, 6), ("fill", (6, 3, 6), 300, 8), ("rim", (3, -8, 4), 200, 4)):
        l = bpy.data.lights.new(name, 'AREA'); l.energy = power * strength * size / 8; l.size = sz
        o = link(bpy.data.objects.new(name, l)); o.location = t + Vector(off)
        o.rotation_euler = (t - o.location).to_track_quat('-Z', 'Y').to_euler()

def project(cam_obj, point, res):
    """World point -> pixel (x, y) in the rendered image."""
    from bpy_extras.object_utils import world_to_camera_view
    sc = bpy.context.scene
    co = world_to_camera_view(sc, cam_obj, Vector(point))
    return (co.x * res[0], (1 - co.y) * res[1])
