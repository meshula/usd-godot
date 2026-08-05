extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/named_material_subsets.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load named material subset fixture", old_value)
        return

    var save_path := "user://named_material_subsets_saved.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save named material subset USDA", old_value)
        return

    var reloaded := load(save_path)
    if reloaded == null or not (reloaded is PackedScene):
        _fail("Failed to reload named material subset USDA", old_value)
        return

    var root := (reloaded as PackedScene).instantiate()
    get_root().add_child(root)

    var mesh_instance := _find_prim_node(root, "/Root/Panel") as MeshInstance3D
    if mesh_instance == null:
        _fail("Failed to find /Root/Panel after reload", old_value)
        return
    if not _require(mesh_instance.mesh != null, "Reloaded named subset mesh is missing", old_value):
        return
    if not _require(mesh_instance.mesh.get_surface_count() == 2, "Reloaded named subset surface count mismatch", old_value):
        return

    var mesh_meta := mesh_instance.get_meta("usd", {}) as Dictionary
    var subset_descriptions := mesh_meta.get("usd:material_subsets", []) as Array
    if not _require(subset_descriptions.size() == 2, "Named subset metadata size mismatch", old_value):
        return

    var first_description := subset_descriptions[0] as Dictionary
    var second_description := subset_descriptions[1] as Dictionary
    if not _require(String(first_description.get("binding_kind", "")) == "mesh", "First subset description should be mesh-bound", old_value):
        return
    if not _require(String(second_description.get("binding_kind", "")) == "subset", "Second subset description should be subset-bound", old_value):
        return
    if not _require(String(second_description.get("subset_path", "")) == "/Root/Panel/Trim", "Subset path was not preserved", old_value):
        return
    if not _require(String(second_description.get("subset_name", "")) == "Trim", "Subset name was not preserved", old_value):
        return
    if not _require(String(second_description.get("family_name", "")) == "materialBind", "Subset family name was not preserved", old_value):
        return

    var material_bindings := mesh_meta.get("usd:material_bindings", []) as Array
    if not _require(material_bindings.size() == 2, "Named subset material binding metadata mismatch", old_value):
        return
    if not _require(String(material_bindings[0]) == "/Root/Looks/Base", "Mesh-bound material path mismatch", old_value):
        return
    if not _require(String(material_bindings[1]) == "/Root/Looks/Accent", "Subset material path mismatch", old_value):
        return

    print("Named material subset metadata round-trip verified.")

    root.free()
    ProjectSettings.set_setting(SETTING, old_value)
    await process_frame
    quit(0)

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

func _require(condition: bool, message: String, old_value: Variant) -> bool:
    if condition:
        return true
    _fail(message, old_value)
    return false

func _fail(message: String, old_value: Variant) -> void:
    push_error(message)
    ProjectSettings.set_setting(SETTING, old_value)
    quit(1)
