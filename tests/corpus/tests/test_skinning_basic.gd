extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/skeleton_skin_basic.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load skeleton_skin_basic.usda", old_value)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)

    var skeleton := _find_prim_node(root, "/Model/Skel") as Skeleton3D
    var mesh_instance := _find_prim_node(root, "/Model/Arm") as MeshInstance3D
    var points_instance := _find_prim_node(root, "/Model/Tips") as MeshInstance3D

    if skeleton == null:
        _fail("Failed to find /Model/Skel", old_value)
        return
    if mesh_instance == null:
        _fail("Failed to find /Model/Arm", old_value)
        return
    if points_instance == null:
        _fail("Failed to find /Model/Tips", old_value)
        return

    if not _require(mesh_instance.get_mesh() != null, "Arm mesh is missing", old_value):
        return
    if not _require(points_instance.get_mesh() != null, "Tips points mesh is missing", old_value):
        return
    if not _require(points_instance.get_mesh().surface_get_primitive_type(0) == Mesh.PRIMITIVE_POINTS, "Tips did not import as Mesh.PRIMITIVE_POINTS", old_value):
        return

    var mesh_meta := mesh_instance.get_meta("usd", {}) as Dictionary
    var points_meta := points_instance.get_meta("usd", {}) as Dictionary
    if not _require(mesh_meta.get("usd:skel_skeleton_path", "") == "/Model/Skel", "Arm skeleton metadata mismatch", old_value):
        return
    if not _require(points_meta.get("usd:skel_skeleton_path", "") == "/Model/Skel", "Tips skeleton metadata mismatch", old_value):
        return
    if not _require(points_meta.get("usd:points_mapping", "") == "mesh_points", "Tips points mapping metadata mismatch", old_value):
        return
    if not _require(int(points_meta.get("usd:point_count", 0)) == 3, "Tips point count metadata mismatch", old_value):
        return

    if not _require(mesh_instance.get_skin() != null, "Arm Skin resource is missing", old_value):
        return
    if not _require(points_instance.get_skin() != null, "Tips Skin resource is missing", old_value):
        return
    if not _require(mesh_instance.get_node_or_null(mesh_instance.get_skeleton_path()) == skeleton, "Arm skeleton path does not resolve to /Model/Skel", old_value):
        return
    if not _require(points_instance.get_node_or_null(points_instance.get_skeleton_path()) == skeleton, "Tips skeleton path does not resolve to /Model/Skel", old_value):
        return

    var mesh_arrays := mesh_instance.get_mesh().surface_get_arrays(0)
    var mesh_bones := mesh_arrays[Mesh.ARRAY_BONES] as PackedInt32Array
    var mesh_weights := mesh_arrays[Mesh.ARRAY_WEIGHTS] as PackedFloat32Array
    if not _require(mesh_bones.size() == 12, "Arm bone array size mismatch", old_value):
        return
    if not _require(mesh_weights.size() == 12, "Arm weight array size mismatch", old_value):
        return
    if not _require(mesh_bones[0] == 0 and is_equal_approx(mesh_weights[0], 1.0), "Arm first vertex influence mismatch", old_value):
        return
    if not _require(mesh_bones[4] == 1 and is_equal_approx(mesh_weights[4], 1.0), "Arm second vertex influence mismatch", old_value):
        return

    var points_arrays := points_instance.get_mesh().surface_get_arrays(0)
    var point_vertices := points_arrays[Mesh.ARRAY_VERTEX] as PackedVector3Array
    var point_colors := points_arrays[Mesh.ARRAY_COLOR] as PackedColorArray
    var point_bones := points_arrays[Mesh.ARRAY_BONES] as PackedInt32Array
    var point_weights := points_arrays[Mesh.ARRAY_WEIGHTS] as PackedFloat32Array
    if not _require(point_vertices.size() == 3, "Tips vertex count mismatch", old_value):
        return
    if not _require(point_colors.size() == 3, "Tips color count mismatch", old_value):
        return
    if not _require(point_bones.size() == 12, "Tips bone array size mismatch", old_value):
        return
    if not _require(point_weights.size() == 12, "Tips weight array size mismatch", old_value):
        return
    if not _require(point_bones[0] == 0 and is_equal_approx(point_weights[0], 1.0), "Tips first point influence mismatch", old_value):
        return
    if not _require(point_bones[4] == 1 and is_equal_approx(point_weights[4], 1.0), "Tips second point influence mismatch", old_value):
        return

    print("Skinned mesh and points bindings verified.")

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
