import unreal
import math

MAP = "/Game/ThirdPerson/Lvl_CollapsePointDemo"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level(MAP)

actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

R0 = unreal.Rotator(0.0, 0.0, 0.0)
GAMEPLAY = {"TestChamber", "PhysicsObject", "CheckpointVolume",
            "CollapseGate", "TriggerButton", "SlideDoor", "ChamberBlock"}

# L / M live in the far corridor beyond K (which ends ~13800).
X_MIN, X_MAX = 13900.0, 16400.0


def V(x, y, z):
    return unreal.Vector(float(x), float(y), float(z))


def set_prop(obj, names, value):
    for n in names:
        try:
            obj.set_editor_property(n, value)
            return True
        except Exception:
            continue
    unreal.log_error("Could not set any of {} on {}".format(names, obj.get_name()))
    return False


def spawn(cls, loc, rot=R0):
    return actor_sub.spawn_actor_from_class(cls, loc, rot)


def label(actor, text):
    try:
        actor.set_actor_label(text, False)
    except Exception:
        pass


# ---- purge any previous L/M bake to avoid duplicates ----
deleted = 0
for a in actor_sub.get_all_level_actors():
    try:
        x = a.get_actor_location().x
    except Exception:
        continue
    if X_MIN <= x <= X_MAX and a.get_class().get_name() in GAMEPLAY:
        actor_sub.destroy_actor(a)
        deleted += 1
unreal.log_error("[BuildLM] purged {} old actors".format(deleted))


def floor(cx, cy, sx, sy):
    blk = spawn(unreal.ChamberBlock, V(cx, cy, -10.0))
    blk.set_actor_scale3d(V(sx, sy, 0.2))
    return blk


def make_chamber(cid, loc, extent):
    ch = spawn(unreal.TestChamber, loc)
    set_prop(ch, ["chamber_id"], unreal.Name(cid))
    box = ch.get_editor_property("bounds")
    if box:
        box.set_box_extent(unreal.Vector(*extent), False)
    label(ch, "CP_Chamber" + cid)
    return ch


def body(loc, name, mass_scale=None):
    b = spawn(unreal.PhysicsObject, loc)
    if mass_scale is not None:
        set_prop(b, ["mass_scale"], float(mass_scale))
    label(b, name)
    return b


def door(loc, name):
    d = spawn(unreal.SlideDoor, loc)
    label(d, name)
    return d


def detonator(loc, name, radius=520.0, mass=12.0):
    g = spawn(unreal.CollapseGate, loc)
    set_prop(g, ["trigger_radius"], float(radius))
    set_prop(g, ["detonate_well", "b_detonate_well"], True)
    set_prop(g, ["detonate_mass_threshold"], float(mass))
    label(g, name)
    return g


def receiver(loc, name, door_actor, impact=1.0, scale=None, scrape_extent=None):
    b = spawn(unreal.TriggerButton, loc)
    set_prop(b, ["min_impact_score"], float(impact))
    set_prop(b, ["linked_door"], door_actor)
    if scale is not None:
        b.set_actor_scale3d(unreal.Vector(*scale))
    if scrape_extent is not None:
        sv = b.get_editor_property("scrape_volume")
        if sv:
            sv.set_box_extent(unreal.Vector(*scrape_extent), False)
    label(b, name)
    return b


# ================= Chamber L: gravitational slingshot =================
# The crosshair-throw only goes down the corridor and hits nothing. A tangential
# release lets the ring's own spin sling the cargo sideways into a side-wall pad.
make_chamber("L", V(14300.0, 0.0, 150.0), (700.0, 900.0, 320.0))
floor(14300.0, 0.0, 12.0, 14.0)
# Light cargo => fast ring => strong tangential sling (do NOT boost mass here).
body(V(14200.0, 0.0, 55.0), "CP_L_Cargo")
door_l = door(V(14800.0, 0.0, 175.0), "CP_L_Door")
# Two long side-wall receivers. Either one opens the door. Only a lateral sling
# can reach them; an aimed throw sails straight down +X and misses both.
receiver(V(14300.0, 560.0, 90.0), "CP_L_SideL", door_l,
         impact=1.0, scale=(9.0, 0.4, 4.0), scrape_extent=(460.0, 70.0, 220.0))
receiver(V(14300.0, -560.0, 90.0), "CP_L_SideR", door_l,
         impact=1.0, scale=(9.0, 0.4, 4.0), scrape_extent=(460.0, 70.0, 220.0))

# ================= Chamber M: cascading collapse (burst) =================
# Over-feed a central pile past the burst threshold; the well detonates and ejects
# mass radially, tripping a whole ring of receivers at once.
make_chamber("M", V(15600.0, 0.0, 160.0), (900.0, 900.0, 360.0))
floor(15600.0, 0.0, 15.0, 15.0)
# 10 boosted bodies gather into a heavy pile. A central detonator waits until the
# well reaches mass 12, then force-bursts it so the ejection is full and wide.
body(V(15600.0, 0.0, 55.0), "CP_M_Feed01", mass_scale=0.1)
RING_N = 9
for i in range(RING_N):
    ang = (2.0 * math.pi * i) / RING_N
    body(V(15600.0 + 200.0 * math.cos(ang), 200.0 * math.sin(ang), 55.0),
         "CP_M_Feed{:02d}".format(i + 2), mass_scale=0.1)
detonator(V(15600.0, 0.0, 90.0), "CP_M_Detonator", radius=520.0, mass=12.0)
door_m = door(V(16300.0, 0.0, 175.0), "CP_M_Door")
# Hexagonal ring of receivers; the radial burst trips several at once. Tall, wide
# pads at floor level catch shards flung out even on shallow arcs.
for i in range(6):
    ang = (2.0 * math.pi * i) / 6
    rx = 15600.0 + 500.0 * math.cos(ang)
    ry = 500.0 * math.sin(ang)
    receiver(V(rx, ry, 55.0), "CP_M_Recv{:02d}".format(i + 1), door_m,
             impact=1.0, scale=(1.8, 1.8, 3.4), scrape_extent=(240.0, 240.0, 300.0))

# ---------------- persist ----------------
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
les.save_current_level()

# ---- post-build census to catch duplicates ----
census = {}
for a in actor_sub.get_all_level_actors():
    try:
        x = a.get_actor_location().x
    except Exception:
        continue
    if X_MIN <= x <= X_MAX and a.get_class().get_name() in GAMEPLAY:
        c = a.get_class().get_name()
        census[c] = census.get(c, 0) + 1
unreal.log_error("[BuildLM] census {}".format(census))
unreal.log_error("[BuildLM] rebuild complete and saved.")
