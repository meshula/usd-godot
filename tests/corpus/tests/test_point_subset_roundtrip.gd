extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/point_subset.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load point subset fixture", old_value)
        return

    var save_path := "user://point_subset_saved.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save point subset USDA", old_value)
        return

    var saved_text := FileAccess.get_file_as_string(save_path)
    if not _require(saved_text.contains("def GeomSubset \"PinnedPoints\""), "Point subset name was not preserved", old_value):
        return
    if not _require(saved_text.contains("uniform token elementType = \"point\""), "Point subset element type was not preserved", old_value):
        return
    if not _require(saved_text.contains("uniform token familyName = \"selection\""), "Point subset family was not preserved", old_value):
        return
    if not _require(saved_text.contains("int[] indices = [1]"), "Point subset indices were not preserved", old_value):
        return

    print("Point subset round-trip verified.")

    ProjectSettings.set_setting(SETTING, old_value)
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
