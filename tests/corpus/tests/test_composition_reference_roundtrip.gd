extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)
    _run_reference_test(old_value)

func _run_reference_test(old_value: Variant) -> void:
    var packed := load("res://samples/composition_reference_source.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load composition reference source fixture", old_value)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)

    var car_node := _find_prim_node(root, "/Root/Car")
    if not _require(car_node != null, "Missing /Root/Car for composition reference scene", old_value):
        return
    var metadata := car_node.get_meta("usd", {}) as Dictionary
    if not _require(String(metadata.get("usd:composition_preservation_mode", "")) == "read_only", "Reference composition mode mismatch", old_value):
        return
    var references := metadata.get("usd:references", []) as Array
    if not _require(references.size() == 1, "Reference metadata size mismatch", old_value):
        return
    var reference := references[0] as Dictionary
    if not _require(String(reference.get("asset_path", "")) == "./composition_ref_target.usda", "Reference asset path mismatch", old_value):
        return
    if not _require(String(reference.get("prim_path", "")) == "/Asset", "Reference prim path mismatch", old_value):
        return
    root.free()

    var save_path := "user://composition_reference_saved.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save composition reference USDA", old_value)
        return

    var saved_text := FileAccess.get_file_as_string(save_path)
    if not _require(saved_text.contains("over \"Car\""), "Saved composition reference is missing over prim", old_value):
        return
    if not _require(saved_text.contains("references = @./composition_ref_target.usda@</Asset>"), "Saved composition reference arc mismatch", old_value):
        return
    if not _require(saved_text.contains("xformOp:transform"), "Saved composition reference transform mismatch", old_value):
        return
    if not _require(not saved_text.contains("def Mesh \"Geom\""), "Saved composition reference should not inline referenced mesh", old_value):
        return

    print("Composition reference round-trip verified.")
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
