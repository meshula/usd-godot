extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/composition_specializes_source.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load composition specializes source fixture", old_value)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)
    var specialized_node := _find_prim_node(root, "/Root/SpecializedAsset")
    if not _require(specialized_node != null, "Missing /Root/SpecializedAsset for composition specializes scene", old_value):
        return
    var metadata := specialized_node.get_meta("usd", {}) as Dictionary
    if not _require(String(metadata.get("usd:composition_preservation_mode", "")) == "read_only", "Specializes composition mode mismatch", old_value):
        return
    var specializes := metadata.get("usd:specializes", []) as Array
    if not _require(specializes.size() == 1 and String(specializes[0]) == "/BaseAsset", "Specializes metadata mismatch", old_value):
        return
    root.free()

    var save_path := "user://composition_specializes_saved.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save composition specializes USDA", old_value)
        return

    var saved_text := FileAccess.get_file_as_string(save_path)
    if not _require(saved_text.contains("def Xform \"SpecializedAsset\""), "Saved specializes prim mismatch", old_value):
        return
    if not _require(saved_text.contains("specializes = </BaseAsset>"), "Saved specializes arc mismatch", old_value):
        return
    if not _require(saved_text.contains("xformOp:transform"), "Saved specializes transform mismatch", old_value):
        return

    print("Composition specializes round-trip verified.")
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

func _find_prim_node(node: Node, prim_path: String) -> Node:
    if node.has_meta("usd"):
        var meta := node.get_meta("usd", {}) as Dictionary
        if meta.get("usd:prim_path", "") == prim_path:
            return node
    for child in node.get_children():
        var found := _find_prim_node(child, prim_path)
        if found != null:
            return found
    return null
