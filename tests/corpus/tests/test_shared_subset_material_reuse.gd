extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var mesh := ArrayMesh.new()
    for surface_index in 3:
        var surface := []
        surface.resize(Mesh.ARRAY_MAX)
        surface[Mesh.ARRAY_VERTEX] = PackedVector3Array([
            Vector3(float(surface_index), 0.0, 0.0),
            Vector3(float(surface_index) + 0.5, 0.0, 0.0),
            Vector3(float(surface_index), 0.5, 0.0),
        ])
        surface[Mesh.ARRAY_NORMAL] = PackedVector3Array([
            Vector3(0.0, 0.0, 1.0),
            Vector3(0.0, 0.0, 1.0),
            Vector3(0.0, 0.0, 1.0),
        ])
        surface[Mesh.ARRAY_INDEX] = PackedInt32Array([0, 1, 2])
        mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, surface)

    var shared_material := StandardMaterial3D.new()
    shared_material.set_albedo(Color(0.8, 0.1, 0.1))
    var unique_material := StandardMaterial3D.new()
    unique_material.set_albedo(Color(0.1, 0.1, 0.8))
    mesh.surface_set_material(0, shared_material)
    mesh.surface_set_material(1, unique_material)
    mesh.surface_set_material(2, shared_material)

    var root := Node3D.new()
    root.name = "Root"
    var mesh_instance := MeshInstance3D.new()
    mesh_instance.name = "TriStrip"
    mesh_instance.mesh = mesh
    root.add_child(mesh_instance)
    mesh_instance.owner = root

    var packed := PackedScene.new()
    if packed.pack(root) != OK:
        root.free()
        _fail("Failed to pack shared subset material scene", old_value)
        return
    root.free()

    var save_path := "user://subset_material_reuse.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save shared subset material USDA", old_value)
        return

    var reloaded := load(save_path)
    if reloaded == null or not (reloaded is PackedScene):
        _fail("Failed to reload shared subset material USDA", old_value)
        return

    var imported_root := (reloaded as PackedScene).instantiate()
    get_root().add_child(imported_root)

    var imported_mesh_instance := _find_prim_node(imported_root, "/Root/TriStrip") as MeshInstance3D
    if imported_mesh_instance == null:
        _fail("Failed to find /Root/TriStrip after reload", old_value)
        return
    if not _require(imported_mesh_instance.mesh != null, "Reloaded shared subset mesh is missing", old_value):
        return
    if not _require(imported_mesh_instance.mesh.get_surface_count() == 3, "Reloaded shared subset mesh surface count mismatch", old_value):
        return

    var mesh_meta := imported_mesh_instance.get_meta("usd", {}) as Dictionary
    var material_bindings := mesh_meta.get("usd:material_bindings", []) as Array
    if not _require(material_bindings.size() == 3, "Reloaded material bindings size mismatch", old_value):
        return
    if not _require(String(material_bindings[0]) == String(material_bindings[2]), "Shared surfaces did not reuse one USD material path", old_value):
        return
    if not _require(String(material_bindings[0]) != String(material_bindings[1]), "Unique surface incorrectly reused shared USD material path", old_value):
        return

    print("Shared subset material reuse verified.")

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
