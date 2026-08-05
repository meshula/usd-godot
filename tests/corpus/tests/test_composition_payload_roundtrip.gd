extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/composition_payload_source.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load composition payload source fixture", old_value)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)
    var payload_node := _find_prim_node(root, "/Root/PayloadCar")
    if not _require(payload_node != null, "Missing /Root/PayloadCar for composition payload scene", old_value):
        return
    var metadata := payload_node.get_meta("usd", {}) as Dictionary
    if not _require(String(metadata.get("usd:composition_preservation_mode", "")) == "read_only", "Payload composition mode mismatch", old_value):
        return
    var payloads := metadata.get("usd:payloads", []) as Array
    if not _require(payloads.size() == 1, "Payload metadata size mismatch", old_value):
        return
    var payload := payloads[0] as Dictionary
    if not _require(String(payload.get("asset_path", "")) == "./composition_payload_target.usda", "Payload asset path mismatch", old_value):
        return
    if not _require(String(payload.get("prim_path", "")) == "/PayloadAsset", "Payload prim path mismatch", old_value):
        return
    root.free()

    var save_path := "user://composition_payload_saved.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save composition payload USDA", old_value)
        return

    var saved_text := FileAccess.get_file_as_string(save_path)
    if not _require(saved_text.contains("over \"PayloadCar\""), "Saved composition payload is missing over prim", old_value):
        return
    if not _require(saved_text.contains("payload = @./composition_payload_target.usda@</PayloadAsset>"), "Saved composition payload arc mismatch", old_value):
        return
    if not _require(saved_text.contains("xformOp:transform"), "Saved composition payload transform mismatch", old_value):
        return
    if not _require(not saved_text.contains("def Mesh \"PayloadGeom\""), "Saved composition payload should not inline payload mesh", old_value):
        return

    print("Composition payload round-trip verified.")
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
