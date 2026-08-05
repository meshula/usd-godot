extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/blend_shape_basic.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load blend_shape_basic.usda", old_value)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)

    var mesh_instance := _find_prim_node(root, "/root/Plane/Plane") as MeshInstance3D
    var skeleton := _find_prim_node(root, "/root/Plane/Skel") as Skeleton3D
    var player := _find_animation_player(root)
    if mesh_instance == null or skeleton == null or player == null:
        _fail("Missing mesh, skeleton, or AnimationPlayer for blend_shape_basic", old_value)
        return

    if not _require(mesh_instance.get_mesh() != null, "Plane mesh is missing", old_value):
        return
    if not _require(mesh_instance.get_mesh().get_blend_shape_count() == 2, "Blend shape count mismatch", old_value):
        return
    if not _require(mesh_instance.find_blend_shape_by_name(&"Key_1") == 0, "Primary blend shape channel missing", old_value):
        return
    if not _require(mesh_instance.find_blend_shape_by_name(&"Key_1__inbetween__HalfKey") == 1, "Inbetween blend shape channel missing", old_value):
        return

    var mesh_meta := mesh_instance.get_meta("usd", {}) as Dictionary
    if not _require(mesh_meta.get("usd:blend_shape_mapping", "") == "array_mesh_relative_piecewise", "Blend shape mapping metadata mismatch", old_value):
        return
    var normal_offsets := mesh_meta.get("usd:blend_shape_has_normal_offsets", {}) as Dictionary
    if not _require(bool(normal_offsets.get("Key_1", false)), "Normal offset metadata missing for Key_1", old_value):
        return
    var channels := mesh_meta.get("usd:blend_shape_channels", {}) as Dictionary
    if not _require(channels.has("Key_1"), "Blend shape channels metadata missing for Key_1", old_value):
        return
    var key_channels := channels["Key_1"] as Array
    if not _require(key_channels.size() == 2, "Key_1 channel metadata size mismatch", old_value):
        return
    var inbetweens := mesh_meta.get("usd:blend_shape_inbetweens", {}) as Dictionary
    if not _require(inbetweens.has("Key_1"), "Inbetween metadata missing for Key_1", old_value):
        return
    var key_inbetweens := inbetweens["Key_1"] as Array
    if not _require(key_inbetweens.size() == 1, "Inbetween metadata size mismatch", old_value):
        return
    var half_key := key_inbetweens[0] as Dictionary
    if not _require(half_key.get("name", "") == "HalfKey", "Inbetween name mismatch", old_value):
        return
    if not _require(is_equal_approx(float(half_key.get("weight", 0.0)), 0.5), "Inbetween weight mismatch", old_value):
        return
    if not _require(int(half_key.get("offset_count", 0)) == 4, "Inbetween offset count mismatch", old_value):
        return
    if not _require(int(half_key.get("normal_offset_count", 0)) == 4, "Inbetween normal offset count mismatch", old_value):
        return

    var blend_shape_arrays := mesh_instance.get_mesh().surface_get_blend_shape_arrays(0)
    if not _require(blend_shape_arrays.size() == 2, "Surface blend shape array count mismatch", old_value):
        return
    var primary_surface := blend_shape_arrays[0] as Array
    var primary_vertices := primary_surface[Mesh.ARRAY_VERTEX] as PackedVector3Array
    var primary_normals := primary_surface[Mesh.ARRAY_NORMAL] as PackedVector3Array
    if not _require(primary_vertices.size() == 6 and is_equal_approx(primary_vertices[0].z, 0.5), "Primary blend shape vertex deltas mismatch", old_value):
        return
    if not _require(primary_normals.size() == 6, "Primary blend shape normal array missing", old_value):
        return
    var inbetween_surface := blend_shape_arrays[1] as Array
    var inbetween_vertices := inbetween_surface[Mesh.ARRAY_VERTEX] as PackedVector3Array
    if not _require(inbetween_vertices.size() == 6 and is_equal_approx(inbetween_vertices[0].z, 0.2), "Inbetween blend shape vertex deltas mismatch", old_value):
        return

    if not _require(player.has_animation(&"Anim"), "Anim clip missing", old_value):
        return
    var animation := player.get_animation(&"Anim")
    var found_primary_track := false
    var found_inbetween_track := false
    for track_index in animation.get_track_count():
        var track_path := str(animation.track_get_path(track_index))
        if animation.track_get_type(track_index) == Animation.TYPE_BLEND_SHAPE and track_path == "root/Plane/Plane:Key_1":
            found_primary_track = true
            if not _require(animation.track_get_key_count(track_index) == 5, "Primary blend shape track key count mismatch", old_value):
                return
            if not _require(is_equal_approx(float(animation.track_get_key_value(track_index, 2)), 0.5), "Primary blend shape track key mismatch", old_value):
                return
        elif animation.track_get_type(track_index) == Animation.TYPE_BLEND_SHAPE and track_path == "root/Plane/Plane:Key_1__inbetween__HalfKey":
            found_inbetween_track = true
            if not _require(animation.track_get_key_count(track_index) == 5, "Inbetween blend shape track key count mismatch", old_value):
                return
            if not _require(is_equal_approx(float(animation.track_get_key_value(track_index, 1)), 1.0), "Inbetween track first active key mismatch", old_value):
                return
            if not _require(is_equal_approx(float(animation.track_get_key_value(track_index, 2)), 0.5), "Inbetween track crossover key mismatch", old_value):
                return

    if not _require(found_primary_track, "Primary blend shape animation track missing", old_value):
        return
    if not _require(found_inbetween_track, "Inbetween blend shape animation track missing", old_value):
        return

    print("Mesh blend shape import verified.")

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

func _find_animation_player(node: Node) -> AnimationPlayer:
    if node is AnimationPlayer:
        return node as AnimationPlayer
    for child in node.get_children():
        var found := _find_animation_player(child)
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
