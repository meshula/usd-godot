extends GutTest
## Tests for USD import functionality
## NOTE: These tests require USD resolver plugins to be properly configured.
## They are currently skipped in headless mode due to ArDefaultResolver issues.


const FIXTURES_PATH = "res://tests/fixtures/"


func test_import_simple_cube_file():
	var doc = UsdDocument.new()
	var state = UsdState.new()
	var parent = Node3D.new()
	add_child_autofree(parent)

	var path = FIXTURES_PATH + "simple_cube.usda"
	var err = doc.import_from_file(path, parent, state)

	assert_eq(err, OK, "Import should succeed for valid USD file")
	assert_gt(parent.get_child_count(), 0, "Should have imported children")
	# RefCounted objects are auto-freed


func test_import_creates_correct_hierarchy():
	var doc = UsdDocument.new()
	var state = UsdState.new()
	var parent = Node3D.new()
	add_child_autofree(parent)

	var path = FIXTURES_PATH + "hierarchy.usda"
	var err = doc.import_from_file(path, parent, state)

	assert_eq(err, OK, "Import should succeed")

	# Check the hierarchy structure
	var root = parent.find_child("Root", false, false)
	assert_not_null(root, "Should have Root node")

	if root:
		var parent_node = root.find_child("Parent", false, false)
		assert_not_null(parent_node, "Should have Parent node under Root")
	# RefCounted objects are auto-freed


func test_import_nonexistent_file_returns_error():
	var doc = UsdDocument.new()
	var state = UsdState.new()
	var parent = Node3D.new()
	add_child_autofree(parent)

	var err = doc.import_from_file("res://nonexistent.usda", parent, state)

	assert_ne(err, OK, "Import should fail for nonexistent file")
	# RefCounted objects are auto-freed


func test_imported_cube_is_mesh_instance():
	var doc = UsdDocument.new()
	var state = UsdState.new()
	var parent = Node3D.new()
	add_child_autofree(parent)

	var path = FIXTURES_PATH + "simple_cube.usda"
	doc.import_from_file(path, parent, state)

	# Find the cube in the hierarchy
	var cube = _find_node_recursive(parent, "TestCube")
	assert_not_null(cube, "Should find TestCube node")

	if cube:
		assert_true(cube is MeshInstance3D, "Cube should be a MeshInstance3D")
	# RefCounted objects are auto-freed


func _find_node_recursive(node: Node, name: String) -> Node:
	if node.name == name:
		return node
	for child in node.get_children():
		var found = _find_node_recursive(child, name)
		if found:
			return found
	return null
