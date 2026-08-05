extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/inherited_material_subset.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load inherited material subset fixture", old_value)
        return

    var loaded_root := (packed as PackedScene).instantiate()
    get_root().add_child(loaded_root)

    var loaded_mesh := _find_prim_node(loaded_root, "/Root/Panel") as MeshInstance3D
    if loaded_mesh == null:
        _fail("Failed to find /Root/Panel in inherited subset source load", old_value)
        return
    if not _require(loaded_mesh.mesh != null, "Inherited subset mesh is missing", old_value):
        return
    if not _require(loaded_mesh.mesh.get_surface_count() == 2, "Inherited subset surface count mismatch", old_value):
        return

    var first_material := loaded_mesh.mesh.surface_get_material(0) as BaseMaterial3D
    var second_material := loaded_mesh.mesh.surface_get_material(1) as BaseMaterial3D
    if not _require(first_material != null and second_material != null, "Inherited subset materials are missing", old_value):
        return
    var first_albedo := first_material.get_albedo()
    var second_albedo := second_material.get_albedo()
    if not _require(is_equal_approx(first_albedo.r, second_albedo.r), "Inherited subset red channel mismatch", old_value):
        return
    if not _require(is_equal_approx(first_albedo.g, second_albedo.g), "Inherited subset green channel mismatch", old_value):
        return
    if not _require(is_equal_approx(first_albedo.b, second_albedo.b), "Inherited subset blue channel mismatch", old_value):
        return

    var mesh_meta := loaded_mesh.get_meta("usd", {}) as Dictionary
    var subset_descriptions := mesh_meta.get("usd:material_subsets", []) as Array
    if not _require(subset_descriptions.size() == 2, "Inherited subset metadata size mismatch", old_value):
        return
    var inherited_description := subset_descriptions[1] as Dictionary
    if not _require(String(inherited_description.get("binding_kind", "")) == "subset", "Inherited subset binding kind mismatch", old_value):
        return
    if not _require(not bool(inherited_description.get("has_material_binding", true)), "Inherited subset should not have its own material binding", old_value):
        return
    if not _require(String(inherited_description.get("subset_name", "")) == "Trim", "Inherited subset name mismatch", old_value):
        return

    loaded_root.free()

    var save_path := "user://inherited_material_subset_saved.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save inherited material subset USDA", old_value)
        return

    var reloaded := load(save_path)
    if reloaded == null or not (reloaded is PackedScene):
        _fail("Failed to reload inherited material subset USDA", old_value)
        return

    var reloaded_root := (reloaded as PackedScene).instantiate()
    get_root().add_child(reloaded_root)

    var reloaded_mesh := _find_prim_node(reloaded_root, "/Root/Panel") as MeshInstance3D
    if reloaded_mesh == null:
        _fail("Failed to find /Root/Panel after inherited subset reload", old_value)
        return

    mesh_meta = reloaded_mesh.get_meta("usd", {}) as Dictionary
    subset_descriptions = mesh_meta.get("usd:material_subsets", []) as Array
    if not _require(subset_descriptions.size() == 2, "Reloaded inherited subset metadata size mismatch", old_value):
        return
    inherited_description = subset_descriptions[1] as Dictionary
    if not _require(String(inherited_description.get("binding_kind", "")) == "subset", "Reloaded inherited subset binding kind mismatch", old_value):
        return
    if not _require(not bool(inherited_description.get("has_material_binding", true)), "Reloaded inherited subset should not have its own material binding", old_value):
        return
    if not _require(String(inherited_description.get("subset_name", "")) == "Trim", "Reloaded inherited subset name mismatch", old_value):
        return

    var material_bindings := mesh_meta.get("usd:material_bindings", []) as Array
    if not _require(material_bindings.size() == 2, "Reloaded inherited subset material binding metadata mismatch", old_value):
        return
    if not _require(String(material_bindings[0]) == "/Root/Looks/Base", "Reloaded mesh binding path mismatch", old_value):
        return
    if not _require(String(material_bindings[1]) == "/Root/Looks/Base", "Reloaded inherited subset should use mesh binding path", old_value):
        return

    print("Inherited material subset metadata round-trip verified.")

    reloaded_root.free()
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
