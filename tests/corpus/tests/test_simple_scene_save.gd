extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var root := Node3D.new()
    root.name = "AuthoredRoot"
    root.transform = Transform3D(Basis.IDENTITY, Vector3(0.5, 0.0, -1.0))

    var box := MeshInstance3D.new()
    box.name = "BoxNode"
    var box_mesh := BoxMesh.new()
    box_mesh.size = Vector3(2.0, 3.0, 4.0)
    box.mesh = box_mesh
    box.transform = Transform3D(Basis.IDENTITY, Vector3(1.25, 2.0, -3.5))
    root.add_child(box)
    box.owner = root

    var sphere := MeshInstance3D.new()
    sphere.name = "SphereNode"
    var sphere_mesh := SphereMesh.new()
    sphere_mesh.radius = 1.5
    sphere.mesh = sphere_mesh
    sphere.transform = Transform3D(Basis.IDENTITY, Vector3(-2.0, 0.75, 0.5))
    root.add_child(sphere)
    sphere.owner = root

    var packed := PackedScene.new()
    if packed.pack(root) != OK:
        root.free()
        _fail("Failed to pack authored scene", old_value)
        return
    root.free()

    var save_path := "user://simple_scene_saved.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save authored USDA", old_value)
        return

    var reloaded := load(save_path)
    if reloaded == null or not (reloaded is PackedScene):
        _fail("Failed to reload authored USDA", old_value)
        return

    var imported_root := (reloaded as PackedScene).instantiate()
    get_root().add_child(imported_root)

    var authored_root := _find_prim_node(imported_root, "/AuthoredRoot") as Node3D
    var imported_box := _find_prim_node(imported_root, "/AuthoredRoot/BoxNode") as MeshInstance3D
    var imported_sphere := _find_prim_node(imported_root, "/AuthoredRoot/SphereNode") as MeshInstance3D

    if authored_root == null:
        _fail("Failed to find /AuthoredRoot after reload", old_value)
        return
    if imported_box == null:
        _fail("Failed to find /AuthoredRoot/BoxNode after reload", old_value)
        return
    if imported_sphere == null:
        _fail("Failed to find /AuthoredRoot/SphereNode after reload", old_value)
        return

    if not _require(authored_root.transform.origin.is_equal_approx(Vector3(0.5, 0.0, -1.0)), "Root transform origin mismatch", old_value):
        return
    if not _require(imported_box.mesh is BoxMesh, "Reloaded box mesh type mismatch", old_value):
        return
    if not _require(imported_sphere.mesh is SphereMesh, "Reloaded sphere mesh type mismatch", old_value):
        return
    if not _require(imported_box.transform.origin.is_equal_approx(Vector3(1.25, 2.0, -3.5)), "Box transform origin mismatch", old_value):
        return
    if not _require(imported_sphere.transform.origin.is_equal_approx(Vector3(-2.0, 0.75, 0.5)), "Sphere transform origin mismatch", old_value):
        return
    if not _require(imported_box.transform.basis.get_scale().is_equal_approx(Vector3(2.0, 3.0, 4.0)), "Box scale mismatch", old_value):
        return
    if not _require(is_equal_approx((imported_sphere.mesh as SphereMesh).radius, 1.5), "Sphere radius mismatch", old_value):
        return

    print("Simple authored USDA save verified.")

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
