extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/skeleton_eight_weights.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load skeleton_eight_weights.usda", old_value)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)

    var skeleton := _find_prim_node(root, "/Model/Skel") as Skeleton3D
    var mesh_instance := _find_prim_node(root, "/Model/Ribbon") as MeshInstance3D
    if skeleton == null:
        _fail("Failed to find /Model/Skel", old_value)
        return
    if mesh_instance == null:
        _fail("Failed to find /Model/Ribbon", old_value)
        return

    if not _require(mesh_instance.get_mesh() != null, "Ribbon mesh is missing", old_value):
        return
    if not _require(mesh_instance.get_skin() != null, "Ribbon Skin resource is missing", old_value):
        return
    if not _require(mesh_instance.get_node_or_null(mesh_instance.get_skeleton_path()) == skeleton, "Ribbon skeleton path does not resolve to /Model/Skel", old_value):
        return

    var surface_format: int = mesh_instance.get_mesh().surface_get_format(0)
    if not _require((surface_format & Mesh.ARRAY_FLAG_USE_8_BONE_WEIGHTS) != 0, "Ribbon did not preserve 8-bone surface format", old_value):
        return

    var arrays := mesh_instance.get_mesh().surface_get_arrays(0)
    var vertices := arrays[Mesh.ARRAY_VERTEX] as PackedVector3Array
    var bones := arrays[Mesh.ARRAY_BONES] as PackedInt32Array
    var weights := arrays[Mesh.ARRAY_WEIGHTS] as PackedFloat32Array
    if not _require(vertices.size() == 3, "Ribbon vertex count mismatch", old_value):
        return
    if not _require(bones.size() == 24, "Ribbon bone array size mismatch", old_value):
        return
    if not _require(weights.size() == 24, "Ribbon weight array size mismatch", old_value):
        return

    var total := 0.0
    for i in range(8):
        total += weights[i]
    if not _require(absf(total - 1.0) < 0.001, "Ribbon first vertex weights are not normalized", old_value):
        return
    if not _require(bones[0] == 0 and bones[1] == 1 and bones[2] == 2 and bones[3] == 3, "Ribbon influence ordering mismatch", old_value):
        return

    print("Eight-weight skinning verified.")

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
