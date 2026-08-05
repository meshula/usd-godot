extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/points_blend_shape_basic.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load points_blend_shape_basic.usda", old_value)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)

    var points_instance := _find_prim_node(root, "/root/Cloud/Cloud") as MeshInstance3D
    var skeleton := _find_prim_node(root, "/root/Cloud/Skel") as Skeleton3D
    var player := _find_animation_player(root)
    if points_instance == null or skeleton == null or player == null:
        _fail("Missing points mesh, skeleton, or AnimationPlayer for points_blend_shape_basic", old_value)
        return

    if not _require(points_instance.get_mesh() != null, "Cloud points mesh is missing", old_value):
        return
    if not _require(points_instance.get_mesh().surface_get_primitive_type(0) == Mesh.PRIMITIVE_POINTS, "Cloud did not import as PRIMITIVE_POINTS", old_value):
        return
    if not _require(points_instance.get_mesh().get_blend_shape_count() == 2, "Cloud blend shape count mismatch", old_value):
        return
    if not _require(points_instance.find_blend_shape_by_name(&"Puff") == 0, "Primary point blend shape channel missing", old_value):
        return
    if not _require(points_instance.find_blend_shape_by_name(&"Puff__inbetween__HalfPuff") == 1, "Point blend shape inbetween channel missing", old_value):
        return

    var points_meta := points_instance.get_meta("usd", {}) as Dictionary
    if not _require(points_meta.get("usd:points_mapping", "") == "mesh_points", "Points mapping metadata mismatch", old_value):
        return
    if not _require(points_meta.get("usd:blend_shape_mapping", "") == "array_points_relative_piecewise", "Point blend shape mapping metadata mismatch", old_value):
        return

    var blend_shape_arrays := points_instance.get_mesh().surface_get_blend_shape_arrays(0)
    if not _require(blend_shape_arrays.size() == 2, "Point blend shape array count mismatch", old_value):
        return
    var primary_surface := blend_shape_arrays[0] as Array
    var primary_vertices := primary_surface[Mesh.ARRAY_VERTEX] as PackedVector3Array
    if not _require(primary_vertices.size() == 4 and is_equal_approx(primary_vertices[0].z, 0.25), "Primary point blend shape deltas mismatch", old_value):
        return
    var inbetween_surface := blend_shape_arrays[1] as Array
    var inbetween_vertices := inbetween_surface[Mesh.ARRAY_VERTEX] as PackedVector3Array
    if not _require(inbetween_vertices.size() == 4 and is_equal_approx(inbetween_vertices[3].z, 0.15), "Point inbetween deltas mismatch", old_value):
        return

    if not _require(player.has_animation(&"Anim"), "Anim clip missing for point blend shape fixture", old_value):
        return
    var animation := player.get_animation(&"Anim")
    var found_primary_track := false
    var found_inbetween_track := false
    for track_index in animation.get_track_count():
        var track_path := str(animation.track_get_path(track_index))
        if animation.track_get_type(track_index) == Animation.TYPE_BLEND_SHAPE and track_path == "root/Cloud/Cloud:Puff":
            found_primary_track = true
        elif animation.track_get_type(track_index) == Animation.TYPE_BLEND_SHAPE and track_path == "root/Cloud/Cloud:Puff__inbetween__HalfPuff":
            found_inbetween_track = true

    if not _require(found_primary_track, "Primary point blend shape track missing", old_value):
        return
    if not _require(found_inbetween_track, "Point blend shape inbetween track missing", old_value):
        return

    print("Point blend shape import verified.")

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
