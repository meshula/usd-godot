extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    ProjectSettings.set_setting(SETTING, 1)

    var packed := load("res://samples/basic.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load basic USD fixture before save", 0)
        return

    var save_path := "user://skip_preview_nodes.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save preview-lighting USD", 0)
        return

    ProjectSettings.set_setting(SETTING, 0)

    var reloaded := load(save_path)
    if reloaded == null or not (reloaded is PackedScene):
        _fail("Failed to reload preview-lighting USD", 0)
        return

    var root := (reloaded as PackedScene).instantiate()
    get_root().add_child(root)

    var root_metadata := root.get_meta("usd", {}) as Dictionary
    if not _require(not bool(root_metadata.get("usd:has_authored_lights", true)), "Reloaded stage should report no authored lights", 0):
        return
    if not _require(not bool(root_metadata.get("usd:has_preview_lighting", true)), "Reloaded stage should not report preview lighting after save", 0):
        return

    print("Preview lighting save skip verified.")
    root.free()
    await process_frame
    quit(0)

func _require(condition: bool, message: String, old_value: Variant) -> bool:
    if condition:
        return true
    _fail(message, old_value)
    return false

func _fail(message: String, old_value: Variant) -> void:
    push_error(message)
    ProjectSettings.set_setting(SETTING, old_value)
    quit(1)
