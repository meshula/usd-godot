extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var first_surface := []
    first_surface.resize(Mesh.ARRAY_MAX)
    first_surface[Mesh.ARRAY_VERTEX] = PackedVector3Array([
        Vector3(0.0, 0.0, 0.0),
        Vector3(1.0, 0.0, 0.0),
        Vector3(0.0, 1.0, 0.0),
    ])
    first_surface[Mesh.ARRAY_NORMAL] = PackedVector3Array([
        Vector3(0.0, 0.0, 1.0),
        Vector3(0.0, 0.0, 1.0),
        Vector3(0.0, 0.0, 1.0),
    ])
    first_surface[Mesh.ARRAY_INDEX] = PackedInt32Array([0, 1, 2])

    var second_surface := []
    second_surface.resize(Mesh.ARRAY_MAX)
    second_surface[Mesh.ARRAY_VERTEX] = PackedVector3Array([
        Vector3(1.0, 0.0, 0.0),
        Vector3(1.0, 1.0, 0.0),
        Vector3(0.0, 1.0, 0.0),
    ])
    second_surface[Mesh.ARRAY_NORMAL] = PackedVector3Array([
        Vector3(0.0, 0.0, 1.0),
        Vector3(0.0, 0.0, 1.0),
        Vector3(0.0, 0.0, 1.0),
    ])
    second_surface[Mesh.ARRAY_INDEX] = PackedInt32Array([0, 1, 2])

    var mesh := ArrayMesh.new()
    mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, first_surface)
    mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, second_surface)

    var red_material := StandardMaterial3D.new()
    red_material.set_albedo(Color(0.8, 0.1, 0.1))
    mesh.surface_set_material(0, red_material)

    var blue_material := StandardMaterial3D.new()
    blue_material.set_albedo(Color(0.1, 0.1, 0.8))
    mesh.surface_set_material(1, blue_material)

    var root := Node3D.new()
    root.name = "Root"
    var mesh_instance := MeshInstance3D.new()
    mesh_instance.name = "Quad"
    mesh_instance.mesh = mesh
    root.add_child(mesh_instance)
    mesh_instance.owner = root

    var packed := PackedScene.new()
    if packed.pack(root) != OK:
        root.free()
        _fail("Failed to pack subset material scene", old_value)
        return
    root.free()

    var save_path := "user://subset_material_roundtrip.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save subset material USDA", old_value)
        return

    var saved_text := FileAccess.get_file_as_string(save_path)
    if not _require(saved_text.contains("def GeomSubset"), "Saved USDA is missing GeomSubset definitions", old_value):
        return

    var reloaded := load(save_path)
    if reloaded == null or not (reloaded is PackedScene):
        _fail("Failed to reload subset material USDA", old_value)
        return

    var imported_root := (reloaded as PackedScene).instantiate()
    get_root().add_child(imported_root)

    var imported_mesh_instance := _find_prim_node(imported_root, "/Root/Quad") as MeshInstance3D
    if imported_mesh_instance == null:
        _fail("Failed to find /Root/Quad after reload", old_value)
        return
    if not _require(imported_mesh_instance.mesh != null, "Reloaded subset mesh is missing", old_value):
        return
    if not _require(imported_mesh_instance.mesh.get_surface_count() == 2, "Reloaded subset mesh surface count mismatch", old_value):
        return

    var first_material := imported_mesh_instance.mesh.surface_get_material(0) as BaseMaterial3D
    var second_material := imported_mesh_instance.mesh.surface_get_material(1) as BaseMaterial3D
    if first_material == null or second_material == null:
        _fail("Reloaded subset surface materials are missing", old_value)
        return
    if not _require(is_equal_approx(first_material.get_albedo().r, 0.8), "Reloaded first material albedo mismatch", old_value):
        return
    if not _require(is_equal_approx(second_material.get_albedo().b, 0.8), "Reloaded second material albedo mismatch", old_value):
        return

    var mesh_meta := imported_mesh_instance.get_meta("usd", {}) as Dictionary
    var material_bindings := mesh_meta.get("usd:material_bindings", []) as Array
    if not _require(material_bindings.size() == 2, "Reloaded material bindings metadata mismatch", old_value):
        return

    print("Subset material USDA round-trip verified.")

    imported_root.free()
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
