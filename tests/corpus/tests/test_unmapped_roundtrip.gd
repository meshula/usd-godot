extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/unmapped_roundtrip.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load unmapped round-trip fixture", old_value)
        return

    var save_path := "user://unmapped_roundtrip_saved.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save unmapped round-trip USDA", old_value)
        return

    var reloaded := load(save_path)
    if reloaded == null or not (reloaded is PackedScene):
        _fail("Failed to reload unmapped round-trip USDA", old_value)
        return

    var root := (reloaded as PackedScene).instantiate()
    get_root().add_child(root)

    var usd_root := _find_prim_node(root, "/Root")
    if not _require(usd_root != null, "Missing /Root after unmapped round-trip reload", old_value):
        return
    var metadata := usd_root.get_meta("usd", {}) as Dictionary
    var unmapped_attributes := metadata.get("usd:unmapped_attributes", {}) as Dictionary
    if not _require(unmapped_attributes.has("debugLabel"), "Missing debugLabel unmapped attribute", old_value):
        return
    if not _require(unmapped_attributes.has("debugDirection"), "Missing debugDirection unmapped attribute", old_value):
        return
    if not _require(unmapped_attributes.has("debugIds"), "Missing debugIds unmapped attribute", old_value):
        return

    var debug_label := unmapped_attributes["debugLabel"] as Dictionary
    if not _require(String(debug_label.get("typed_value_kind", "")) == "string", "debugLabel typed kind mismatch", old_value):
        return
    if not _require(String(debug_label.get("typed_value", "")) == "car", "debugLabel value mismatch", old_value):
        return

    var debug_direction := unmapped_attributes["debugDirection"] as Dictionary
    if not _require(String(debug_direction.get("typed_value_kind", "")) == "vector3", "debugDirection typed kind mismatch", old_value):
        return
    if not _require((debug_direction.get("typed_value", Vector3()) as Vector3).is_equal_approx(Vector3(1.0, 2.0, 3.0)), "debugDirection value mismatch", old_value):
        return

    var debug_ids := unmapped_attributes["debugIds"] as Dictionary
    if not _require(String(debug_ids.get("typed_value_kind", "")) == "int_array", "debugIds typed kind mismatch", old_value):
        return
    var ids := debug_ids.get("typed_value", []) as Array
    if not _require(ids.size() == 3, "debugIds array length mismatch", old_value):
        return
    if not _require(int(ids[0]) == 1 and int(ids[1]) == 3 and int(ids[2]) == 5, "debugIds values mismatch", old_value):
        return

    var unmapped_relationships := metadata.get("usd:unmapped_relationships", {}) as Dictionary
    if not _require(unmapped_relationships.has("debugTarget"), "Missing debugTarget relationship", old_value):
        return
    var debug_target := unmapped_relationships["debugTarget"] as Dictionary
    if not _require(bool(debug_target.get("is_custom", false)), "debugTarget custom flag mismatch", old_value):
        return
    var targets := debug_target.get("targets", []) as Array
    if not _require(targets.size() == 1 and String(targets[0]) == "/Root/Target", "debugTarget targets mismatch", old_value):
        return

    print("Unmapped attribute and relationship round-trip verified.")

    root.free()
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
