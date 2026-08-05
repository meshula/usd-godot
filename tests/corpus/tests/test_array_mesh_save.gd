extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var root := Node3D.new()
    root.name = "ArrayRoot"

    var mesh := ArrayMesh.new()
    var arrays := []
    arrays.resize(Mesh.ARRAY_MAX)
    arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([
        Vector3(0.0, 0.0, 0.0),
        Vector3(1.0, 0.0, 0.0),
        Vector3(0.0, 1.0, 0.0),
    ])
    arrays[Mesh.ARRAY_NORMAL] = PackedVector3Array([
        Vector3(0.0, 0.0, 1.0),
        Vector3(0.0, 0.0, 1.0),
        Vector3(0.0, 0.0, 1.0),
    ])
    arrays[Mesh.ARRAY_TEX_UV] = PackedVector2Array([
        Vector2(0.0, 0.0),
        Vector2(1.0, 0.0),
        Vector2(0.0, 1.0),
    ])
    arrays[Mesh.ARRAY_INDEX] = PackedInt32Array([0, 1, 2])
    mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)

    var material := StandardMaterial3D.new()
    material.set_albedo(Color(0.3, 0.6, 0.9, 1.0))
    material.set_roughness(0.25)
    mesh.surface_set_material(0, material)

    var instance := MeshInstance3D.new()
    instance.name = "Triangle"
    instance.mesh = mesh
    root.add_child(instance)
    instance.owner = root

    var packed := PackedScene.new()
    if packed.pack(root) != OK:
        root.free()
        _fail("Failed to pack ArrayMesh scene", old_value)
        return
    root.free()

    var save_path := "user://array_mesh_saved.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save ArrayMesh USDA", old_value)
        return

    var saved_text := FileAccess.get_file_as_string(save_path)
    if not _require(saved_text.contains("def Mesh \"Triangle\""), "Saved USDA is missing mesh prim", old_value):
        return
    if not _require(saved_text.contains("faceVertexCounts"), "Saved USDA is missing faceVertexCounts", old_value):
        return
    if not _require(saved_text.contains("faceVertexIndices"), "Saved USDA is missing faceVertexIndices", old_value):
        return
    if not _require(saved_text.contains("primvars:st"), "Saved USDA is missing primvars:st", old_value):
        return
    if not _require(saved_text.contains("normals"), "Saved USDA is missing normals", old_value):
        return
    if not _require(saved_text.contains("rel material:binding"), "Saved USDA is missing material binding", old_value):
        return

    var reloaded := load(save_path)
    if reloaded == null or not (reloaded is PackedScene):
        _fail("Failed to reload ArrayMesh USDA", old_value)
        return

    var imported_root := (reloaded as PackedScene).instantiate()
    get_root().add_child(imported_root)

    var imported_mesh_instance := _find_prim_node(imported_root, "/ArrayRoot/Triangle") as MeshInstance3D
    if imported_mesh_instance == null:
        _fail("Failed to find /ArrayRoot/Triangle after reload", old_value)
        return

    var imported_mesh := imported_mesh_instance.mesh
    if imported_mesh == null:
        _fail("Reloaded mesh is missing", old_value)
        return
    if not _require(imported_mesh.get_surface_count() == 1, "Reloaded mesh surface count mismatch", old_value):
        return

    var imported_arrays := imported_mesh.surface_get_arrays(0)
    var vertices := imported_arrays[Mesh.ARRAY_VERTEX] as PackedVector3Array
    var normals := imported_arrays[Mesh.ARRAY_NORMAL] as PackedVector3Array
    var uvs := imported_arrays[Mesh.ARRAY_TEX_UV] as PackedVector2Array
    var indices := imported_arrays[Mesh.ARRAY_INDEX] as PackedInt32Array
    if not _require(vertices.size() == 3, "Reloaded vertex count mismatch", old_value):
        return
    if not _require(normals.size() == 3, "Reloaded normal count mismatch", old_value):
        return
    if not _require(uvs.size() == 3, "Reloaded UV count mismatch", old_value):
        return
    if not _require(indices.size() == 3, "Reloaded index count mismatch", old_value):
        return
    if not _require(vertices[1].is_equal_approx(Vector3(1.0, 0.0, 0.0)), "Reloaded vertex data mismatch", old_value):
        return
    if not _require(_contains_uv(uvs, Vector2(0.0, 1.0)) and _contains_uv(uvs, Vector2(1.0, 1.0)) and _contains_uv(uvs, Vector2(0.0, 0.0)), "Reloaded UV data mismatch", old_value):
        return

    var loaded_material := imported_mesh_instance.get_active_material(0) as BaseMaterial3D
    if loaded_material == null:
        _fail("Reloaded ArrayMesh material is missing", old_value)
        return
    if not _require(is_equal_approx(loaded_material.get_albedo().r, 0.3), "Reloaded material albedo.r mismatch", old_value):
        return
    if not _require(is_equal_approx(loaded_material.get_roughness(), 0.25), "Reloaded material roughness mismatch", old_value):
        return

    print("ArrayMesh USDA round-trip verified.")

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

func _contains_uv(uvs: PackedVector2Array, value: Vector2) -> bool:
    for uv in uvs:
        if uv.is_equal_approx(value):
            return true
    return false

func _fail(message: String, old_value: Variant) -> void:
    push_error(message)
    ProjectSettings.set_setting(SETTING, old_value)
    quit(1)
