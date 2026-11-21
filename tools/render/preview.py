"""Quick look render of one location with speaker + mic models placed in the scene.
usage: python3 preview.py <spec.json> <out.png> [azimuth] [elevation] [samples] [speaker spot] [mic spot]"""
import sys, json, math, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
from mathutils import Vector
import srlib, models

spec = json.load(open(sys.argv[1])); out = sys.argv[2]
az = float(sys.argv[3]) if len(sys.argv) > 3 else 215
el = float(sys.argv[4]) if len(sys.argv) > 4 else 42
samples = int(sys.argv[5]) if len(sys.argv) > 5 else 32
spk_id = sys.argv[6] if len(sys.argv) > 6 else "s_rear_right"
mic_id = sys.argv[7] if len(sys.argv) > 7 else "m_driver"

sc = srlib.reset(); sc.cycles.samples = samples
sc.render.resolution_x, sc.render.resolution_y = 1400, 875
info = models.car(spec["car"])
spots = spec["spots"]
sp, mp = Vector(spots[spk_id]["pos"]), Vector(spots[mic_id]["pos"])
yaw_s = math.atan2(mp.y - sp.y, mp.x - sp.x)
models.speaker((sp.x, sp.y, sp.z - 0.27), yaw_s, scale=1.5)
models.microphone((mp.x, mp.y, mp.z - 0.6), yaw_s + math.pi, scale=1.5)
# ground shadow catcher
g = srlib.box("ground", (0, 0, -0.01), (14, 14, 0.02), srlib.mat("ground", (0.8, 0.8, 0.8)))
g.is_shadow_catcher = True
bpy.data.objects["cam"] if False else None
cam = srlib.iso_camera(info["center"], info["extent"], az, el)
srlib.lights(info["center"], 6)
sc.render.filepath = out
bpy.ops.render.render(write_still=True)
print("rendered", out)
