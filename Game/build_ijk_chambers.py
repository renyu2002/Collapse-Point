import unreal
import math

MAP = "/Game/ThirdPerson/Lvl_CollapsePointDemo"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level(MAP)

actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

R0 = unreal.Rotator(0.0, 0.0, 0.0)
GAMEPLAY = {"TestChamber", "PhysicsObject", "CheckpointVolume",
            "CollapseGate", "TriggerButton", "SlideDoor", "ChamberBlock"}


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


# ---- purge any previous I/J/K bake in the far corridor to avoid duplicates ----
deleted = 0
for a in actor_sub.get_all_level_actors():
    try:
        x = a.get_actor_location().x
    except Exception:
        continue
    if 10000.0 <= x <= 13800.0 and a.get_class().get_name() in GAMEPLAY:
        actor_sub.destroy_actor(a)
        deleted += 1
unreal.log_error("[BuildIJK] purged {} old actors".format(deleted))


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


def feed(loc, name):
    b = spawn(unreal.PhysicsObject, loc)
    # Default light cubes weigh ~0.37 suck-mass; boost so a cluster of six
    # reliably crosses the 8.0 black-hole threshold (capped at 2.0 each -> 12).
    set_prop(b, ["mass_scale"], 0.1)
    label(b, name)
    return b


def ring(cx, cy, cz, radius, count, prefix, start_index=1):
    for i in range(count):
        ang = (2.0 * math.pi * i) / count
        feed(V(cx + radius * math.cos(ang), cy + radius * math.sin(ang), cz),
             "{}{:02d}".format(prefix, start_index + i))


def containment(loc, extent):
    v = spawn(unreal.CheckpointVolume, loc)
    set_prop(v, ["invert_containment", "b_invert_containment"], True)
    tb = v.get_editor_property("trigger_box")
    if tb:
        tb.set_box_extent(unreal.Vector(*extent), False)
    label(v, "CP_ContainmentCell")
    return v


def door(loc, name):
    d = spawn(unreal.SlideDoor, loc)
    label(d, name)
    return d


def button(loc, name, door_actor, impact=4000.0, scale=None, scrape_extent=None):
    b = spawn(unreal.TriggerButton, loc)
    set_prop(b, ["min_impact_score"], float(impact))
    set_prop(b, ["mass_receiver_extent"], unreal.Vector(280.0, 280.0, 140.0))
    set_prop(b, ["linked_door"], door_actor)
    if scale is not None:
        b.set_actor_scale3d(unreal.Vector(*scale))
    if scrape_extent is not None:
        sv = b.get_editor_property("scrape_volume")
        if sv:
            sv.set_box_extent(unreal.Vector(*scrape_extent), False)
    label(b, name)
    return b


def gate(loc, name, door_actor, radius=750.0):
    g = spawn(unreal.CollapseGate, loc)
    set_prop(g, ["trigger_radius"], float(radius))
    set_prop(g, ["linked_door"], door_actor)
    label(g, name)
    return g


# ---------------- Chamber I: collapse gate (swallow-as-key) ----------------
make_chamber("I", V(10400.0, 0.0, 120.0), (600.0, 600.0, 300.0))
floor(10500.0, 0.0, 12.0, 10.0)
feed(V(10400.0, 0.0, 55.0), "CP_I_Feed01")
ring(10400.0, 0.0, 55.0, 240.0, 5, "CP_I_Feed", start_index=2)
door_i = door(V(10950.0, 0.0, 175.0), "CP_I_Door")
gate(V(10850.0, 0.0, 90.0), "CP_I_Gate", door_i, radius=700.0)

# ---------------- Chamber J: inverted containment (field is the workbench) ----
make_chamber("J", V(11800.0, 0.0, 140.0), (900.0, 600.0, 340.0))
floor(11900.0, 0.0, 20.0, 10.0)
containment(V(11600.0, 0.0, 140.0), (200.0, 260.0, 200.0))
# A small cluster of cargo (not a lone cube): flung together, at least one shard
# reliably crosses the field into the receiver — removes single-body fling luck.
feed(V(11600.0, 0.0, 55.0), "CP_J_Cargo")
feed(V(11540.0, -70.0, 55.0), "CP_J_Cargo02")
feed(V(11540.0, 70.0, 55.0), "CP_J_Cargo03")
door_j = door(V(12550.0, 0.0, 175.0), "CP_J_Door")
# Receiver zone spans the ENTIRE exit corridor, its inner face flush with the
# containment cell wall (cell ends at X=11800). Any shard that leaves the cell
# — at any height, anywhere down the corridor — trips it. This makes the puzzle
# deterministic: "get cargo out of the cell" IS the win, not a lucky landing.
button(V(12400.0, 0.0, 20.0), "CP_J_Receiver", door_j,
       impact=1.0, scrape_extent=(600.0, 550.0, 320.0))

# ---------------- Chamber K: capstone (containment + collapse gate) ----------
make_chamber("K", V(12900.0, 0.0, 160.0), (700.0, 700.0, 340.0))
floor(13000.0, 0.0, 16.0, 12.0)
containment(V(12900.0, 0.0, 160.0), (360.0, 360.0, 240.0))
feed(V(12900.0, 0.0, 55.0), "CP_K_Feed01")
ring(12900.0, 0.0, 55.0, 230.0, 5, "CP_K_Feed", start_index=2)
door_k = door(V(13400.0, 0.0, 175.0), "CP_K_Door")
gate(V(13300.0, 0.0, 90.0), "CP_K_Gate", door_k, radius=800.0)

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
    if 10000.0 <= x <= 13800.0 and a.get_class().get_name() in GAMEPLAY:
        c = a.get_class().get_name()
        census[c] = census.get(c, 0) + 1
unreal.log_error("[BuildIJK] census {}".format(census))
unreal.log_error("[BuildIJK] rebuild complete and saved.")
