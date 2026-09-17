import unreal

MAP = "/Game/ThirdPerson/Lvl_CollapsePointDemo"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level(MAP)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_sub.get_all_level_actors()


def near(x, lo, hi):
    return lo <= x <= hi


buckets = {"I": (10000, 11100), "J": (11100, 12400), "K": (12200, 13800)}
for cid, (lo, hi) in buckets.items():
    unreal.log("==== Chamber {} ({}..{}) ====".format(cid, lo, hi))
    for a in actors:
        loc = a.get_actor_location()
        if not near(loc.x, lo, hi):
            continue
        cls = a.get_class().get_name()
        if cls in ("StaticMeshActor",):
            continue
        extra = ""
        if cls == "PhysicsObject":
            try:
                extra = " suck={} mass<= {}".format(
                    a.get_editor_property("can_be_sucked"),
                    a.get_editor_property("max_mass_contribution"))
            except Exception:
                pass
        if cls == "CheckpointVolume":
            try:
                extra = " invert={}".format(a.get_editor_property("invert_containment"))
            except Exception:
                pass
        unreal.log("  {:16s} ({:6.0f},{:6.0f},{:6.0f}) {}".format(
            cls, loc.x, loc.y, loc.z, extra))
