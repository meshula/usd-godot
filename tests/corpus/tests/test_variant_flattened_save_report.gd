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

func _run_test() -> void:
	var flattened_path := "user://variant_flattened.usdz"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(flattened_path))

	var packed_scene: PackedScene = ResourceLoader.load("res://samples/variant_stage.usda", "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	_require(packed_scene != null, "Failed to load variant USD layer as PackedScene.")

	var root := packed_scene.instantiate()
	_require(root is UsdStageInstance, "Variant USD layer did not instantiate as a UsdStageInstance root.")
	var stage_instance := root as UsdStageInstance
	stage_instance.set("variants/Model/modelingVariant", "blue")

	_require(_save_instantiated_scene(root, flattened_path) == OK, "Flattened USDZ composed export should save successfully.")
	await _cleanup_node(root)

	var flattened_scene: PackedScene = ResourceLoader.load(flattened_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	_require(flattened_scene != null, "Failed to reload flattened USDZ export.")
	root = flattened_scene.instantiate()
	_require(not root is UsdStageInstance, "Flattened USDZ export should reload as a composed static scene, not a live UsdStageInstance.")
	_require(root.get_node_or_null("Model/BlueSphere") != null, "Flattened USDZ export should contain the selected composed branch.")
	_require(root.get_node_or_null("Model/RedCube") == null, "Flattened USDZ export should not contain inactive variant branches.")
	await _cleanup_node(root)

	quit(1 if failed else 0)
