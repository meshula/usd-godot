extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/reset_stage_transform.usda")
    if packed == null or not (packed is PackedScene):
        push_error("Failed to load reset_stage_transform.usda")
        ProjectSettings.set_setting(SETTING, old_value)
        quit(1)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)

    var generated := root.get_child(0) as Node3D
    print("Generated scale: %s" % generated.transform.basis.get_scale())
    var authored_root := generated.get_child(0) as Node3D
    var parent := authored_root.get_child(0) as Node3D
    var reset_branch := parent.get_child(0) as Node3D

    print("Reset top-level: %s" % reset_branch.is_set_as_top_level())
    print("Reset transform: %s" % reset_branch.transform)
    if reset_branch.has_meta("usd"):
        print("Reset USD meta: %s" % reset_branch.get_meta("usd"))

    root.free()
    ProjectSettings.set_setting(SETTING, old_value)
    await process_frame
    quit(0)
