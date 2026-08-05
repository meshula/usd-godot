extends SceneTree

var failed := false

func _initialize() -> void:
	call_deferred("_run_test")

func _fail(message: String) -> void:
	failed = true
	push_error(message)

func _require(condition: bool, message: String) -> void:
	if not condition:
		_fail(message)

func _save_instantiated_scene(root: Node, path: String) -> int:
	var saved_scene := PackedScene.new()
	var pack_error := saved_scene.pack(root)
	if pack_error != OK:
		return pack_error
	return ResourceSaver.save(saved_scene, path)

func _cleanup_node(node: Node) -> void:
	if node == null:
		return
	for child in node.get_children(true):
		if child.name == "_Generated" or child.has_meta("usd_stage_instance_generated"):
			node.remove_child(child)
			child.free()
	if node is UsdStageInstance:
		(node as UsdStageInstance).stage = null
	if node.get_parent() != null:
		node.get_parent().remove_child(node)
	node.free()
	await process_frame

func _own_recursive(node: Node, owner: Node) -> void:
	if node != owner:
		node.owner = owner
	for child in node.get_children():
		_own_recursive(child, owner)

func _run_test() -> void:
	var edited_path := "user://variant_boundary_edit.usda"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(edited_path))

	var packed_scene: PackedScene = ResourceLoader.load("res://samples/variant_stage.usda", "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	_require(packed_scene != null, "Failed to load variant USD layer as PackedScene.")

	var root := packed_scene.instantiate()
	_require(root is UsdStageInstance, "Variant USD layer did not instantiate as a UsdStageInstance root.")
	var stage_instance := root as UsdStageInstance
	stage_instance.set("variants/Model/modelingVariant", "blue")

	var blue_sphere := stage_instance.get_node_for_prim_path("/Model/BlueSphere")
	_require(blue_sphere is Node3D, "BlueSphere node was not generated after selecting the blue variant.")
	(blue_sphere as Node3D).position += Vector3(3.0, 0.0, 0.0)
	_own_recursive(root, root)

	var save_error := _save_instantiated_scene(root, edited_path)
	_require(save_error == OK, "Saving a source USD layer with a generated edit should still complete.")
	await _cleanup_node(root)

	var edited_packed_scene: PackedScene = ResourceLoader.load(edited_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	_require(edited_packed_scene != null, "Failed to reload edited USD layer as PackedScene.")
	root = edited_packed_scene.instantiate()
	_require(root is UsdStageInstance, "Edited USD layer did not instantiate as a UsdStageInstance root.")
	stage_instance = root as UsdStageInstance
	stage_instance.rebuild()
	_require(stage_instance.get("variants/Model/modelingVariant") == "blue", "Edited USD layer did not preserve the selected variant default.")
	_require(stage_instance.get_node_for_prim_path("/Model/BlueSphere") != null, "Edited USD layer did not load the selected blue branch.")
	await _cleanup_node(root)

	quit(1 if failed else 0)
