extends GutTest
## Tests for UsdPrimProxy - the ergonomic GDScript wrapper for UsdPrim
## NOTE: These tests define the expected API. They will fail until UsdPrimProxy is implemented.


const FIXTURES_PATH = "res://tests/fixtures/"


# -----------------------------------------------------------------------------
# Prim Properties Tests
# -----------------------------------------------------------------------------

func test_prim_get_name():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var prim = stage.get_prim_at_path("/World/TestCube")
	assert_eq(prim.get_name(), "TestCube")
	# RefCounted objects are auto-freed


func test_prim_get_path():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var prim = stage.get_prim_at_path("/World/TestCube")
	assert_eq(prim.get_path(), "/World/TestCube")
	# RefCounted objects are auto-freed


func test_prim_get_type_name():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var prim = stage.get_prim_at_path("/World/TestCube")
	assert_eq(prim.get_type_name(), "Cube")
	# RefCounted objects are auto-freed


func test_prim_is_valid():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var prim = stage.get_prim_at_path("/World/TestCube")
	assert_true(prim.is_valid(), "Existing prim should be valid")
	# RefCounted objects are auto-freed


# -----------------------------------------------------------------------------
# Hierarchy Tests
# -----------------------------------------------------------------------------

func test_prim_get_parent():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "hierarchy.usda")
	var child = stage.get_prim_at_path("/Root/Parent/Child")
	assert_not_null(child, "Should find Child prim")
	if child:
		var parent_prim = child.get_parent()
		assert_not_null(parent_prim, "Child should have a parent")
		if parent_prim:
			assert_eq(parent_prim.get_name(), "Parent")
	# RefCounted objects are auto-freed


func test_prim_get_children():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var world = stage.get_prim_at_path("/World")
	var children = world.get_children()
	assert_eq(children.size(), 2, "World should have 2 children")
	# RefCounted objects are auto-freed


func test_prim_has_child():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var world = stage.get_prim_at_path("/World")
	assert_true(world.has_child("TestCube"))
	assert_false(world.has_child("NonExistent"))
	# RefCounted objects are auto-freed


# -----------------------------------------------------------------------------
# Attribute Tests
# -----------------------------------------------------------------------------

func test_prim_get_attribute_names():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var cube = stage.get_prim_at_path("/World/TestCube")
	var attr_names = cube.get_attribute_names()
	assert_true("size" in attr_names, "Cube should have 'size' attribute")
	# RefCounted objects are auto-freed


func test_prim_get_attribute_value():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var cube = stage.get_prim_at_path("/World/TestCube")
	var size = cube.get_attribute("size")
	assert_eq(size, 2.0, "Cube size should be 2.0")
	# RefCounted objects are auto-freed


func test_prim_set_attribute_value():
	var stage = UsdStageProxy.new()
	stage.create_new("res://tests/output/attr_test.usda")
	var prim = stage.define_prim("/TestCube", "Cube")
	assert_not_null(prim, "Should create prim")
	if prim:
		prim.set_attribute("size", 3.0)
		assert_eq(prim.get_attribute("size"), 3.0)
	# RefCounted objects are auto-freed


func test_prim_has_attribute():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var cube = stage.get_prim_at_path("/World/TestCube")
	assert_true(cube.has_attribute("size"))
	assert_false(cube.has_attribute("nonexistent"))
	# RefCounted objects are auto-freed


# -----------------------------------------------------------------------------
# Transform Tests
# -----------------------------------------------------------------------------

func test_prim_get_local_transform():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var cube = stage.get_prim_at_path("/World/TestCube")
	var xform = cube.get_local_transform()
	assert_typeof(xform, TYPE_TRANSFORM3D)
	# TestCube is translated to (0, 1, 0)
	assert_eq(xform.origin, Vector3(0, 1, 0))
	# RefCounted objects are auto-freed


func test_prim_get_world_transform():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "hierarchy.usda")
	var grandchild = stage.get_prim_at_path("/Root/Parent/Child/GrandchildCube")
	assert_not_null(grandchild, "Should find GrandchildCube")
	if grandchild:
		var world_xform = grandchild.get_world_transform()
		# Parent: (1, 0, 0), Child: (0, 2, 0) -> World: (1, 2, 0)
		assert_eq(world_xform.origin, Vector3(1, 2, 0))
	# RefCounted objects are auto-freed


func test_prim_set_local_transform():
	var stage = UsdStageProxy.new()
	stage.create_new("res://tests/output/xform_test.usda")
	var prim = stage.define_prim("/TestXform", "Xform")
	assert_not_null(prim, "Should create prim")
	if prim:
		var xform = Transform3D()
		xform.origin = Vector3(5, 10, 15)
		prim.set_local_transform(xform)
		var retrieved = prim.get_local_transform()
		assert_eq(retrieved.origin, Vector3(5, 10, 15))
	# RefCounted objects are auto-freed


# -----------------------------------------------------------------------------
# Variant Tests
# -----------------------------------------------------------------------------

func test_prim_get_variant_sets():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "with_variants.usda")
	var prim = stage.get_prim_at_path("/Model")
	assert_not_null(prim, "Should find Model prim")
	if prim:
		var variant_sets = prim.get_variant_sets()
		assert_true("LOD" in variant_sets, "Model should have LOD variant set")
	# RefCounted objects are auto-freed


func test_prim_set_variant_selection():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "with_variants.usda")
	var prim = stage.get_prim_at_path("/Model")
	assert_not_null(prim, "Should find Model prim")
	if prim:
		prim.set_variant_selection("LOD", "low")
		assert_eq(prim.get_variant_selection("LOD"), "low")
	# RefCounted objects are auto-freed


# -----------------------------------------------------------------------------
# Creation Tests (via UsdStageProxy)
# -----------------------------------------------------------------------------

func test_define_prim_creates_new_prim():
	var stage = UsdStageProxy.new()
	stage.create_new("res://tests/output/define_prim_test.usda")
	var prim = stage.define_prim("/MyXform", "Xform")
	assert_not_null(prim)
	assert_true(prim.is_valid())
	assert_eq(prim.get_type_name(), "Xform")
	# RefCounted objects are auto-freed


func test_define_nested_prim():
	var stage = UsdStageProxy.new()
	stage.create_new("res://tests/output/nested_prim_test.usda")
	stage.define_prim("/Root", "Xform")
	stage.define_prim("/Root/Child", "Cube")
	var child = stage.get_prim_at_path("/Root/Child")
	assert_not_null(child)
	assert_eq(child.get_type_name(), "Cube")
	# RefCounted objects are auto-freed
