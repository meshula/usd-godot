extends GutTest
## Tests for UsdStageProxy - the ergonomic GDScript wrapper for UsdStage
## NOTE: These tests define the expected API. They will fail until UsdStageProxy is implemented.


const FIXTURES_PATH = "res://tests/fixtures/"


# -----------------------------------------------------------------------------
# Stage Lifecycle Tests
# -----------------------------------------------------------------------------

func test_stage_proxy_can_be_instantiated():
	var stage = UsdStageProxy.new()
	assert_not_null(stage, "UsdStageProxy should be instantiable")
	# RefCounted objects are auto-freed


func test_stage_open_valid_file():
	var stage = UsdStageProxy.new()
	var err = stage.open(FIXTURES_PATH + "simple_cube.usda")
	assert_eq(err, OK, "Should open valid USD file")
	assert_true(stage.is_open(), "Stage should be marked as open")
	# RefCounted objects are auto-freed


func test_stage_open_invalid_file_returns_error():
	var stage = UsdStageProxy.new()
	var err = stage.open("res://nonexistent.usda")
	assert_ne(err, OK, "Should return error for invalid file")
	assert_false(stage.is_open(), "Stage should not be open")
	# RefCounted objects are auto-freed


func test_stage_create_new():
	var stage = UsdStageProxy.new()
	var err = stage.create_new("res://tests/output/new_stage.usda")
	assert_eq(err, OK, "Should create new stage")
	assert_true(stage.is_open(), "Stage should be open after creation")
	# RefCounted objects are auto-freed


func test_stage_save():
	var stage = UsdStageProxy.new()
	stage.create_new("res://tests/output/save_test.usda")
	var err = stage.save()
	assert_eq(err, OK, "Should save successfully")
	# RefCounted objects are auto-freed


func test_stage_close():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	stage.close()
	assert_false(stage.is_open(), "Stage should be closed")
	# RefCounted objects are auto-freed


# -----------------------------------------------------------------------------
# Prim Access Tests
# -----------------------------------------------------------------------------

func test_get_default_prim():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var prim = stage.get_default_prim()
	assert_not_null(prim, "Should return default prim")
	assert_eq(prim.get_name(), "World", "Default prim should be 'World'")
	# RefCounted objects are auto-freed


func test_get_prim_at_path():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var prim = stage.get_prim_at_path("/World/TestCube")
	assert_not_null(prim, "Should find prim at path")
	assert_eq(prim.get_name(), "TestCube", "Prim name should match")
	# RefCounted objects are auto-freed


func test_get_prim_invalid_path_returns_null():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var prim = stage.get_prim_at_path("/World/NonExistent")
	assert_null(prim, "Should return null for invalid path")
	# RefCounted objects are auto-freed


func test_traverse_returns_all_prims():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var prims = stage.traverse()
	assert_gt(prims.size(), 0, "Should return prims")
	# simple_cube.usda has: World, TestCube, TestSphere = 3 prims
	assert_eq(prims.size(), 3, "Should have expected number of prims")
	# RefCounted objects are auto-freed


# -----------------------------------------------------------------------------
# Time Code Tests
# -----------------------------------------------------------------------------

func test_set_and_get_time_code():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	stage.set_time_code(24.0)
	assert_eq(stage.get_time_code(), 24.0, "Time code should be settable")
	# RefCounted objects are auto-freed


func test_get_time_range():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var start_time = stage.get_start_time_code()
	var end_time = stage.get_end_time_code()
	assert_typeof(start_time, TYPE_FLOAT)
	assert_typeof(end_time, TYPE_FLOAT)
	# RefCounted objects are auto-freed


# -----------------------------------------------------------------------------
# Metadata Tests
# -----------------------------------------------------------------------------

func test_get_root_layer_path():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var path = stage.get_root_layer_path()
	assert_true(path.ends_with("simple_cube.usda"), "Should return file path")
	# RefCounted objects are auto-freed


func test_get_up_axis():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var axis = stage.get_up_axis()
	assert_eq(axis, "Y", "Up axis should be Y")
	# RefCounted objects are auto-freed


func test_get_meters_per_unit():
	var stage = UsdStageProxy.new()
	stage.open(FIXTURES_PATH + "simple_cube.usda")
	var mpu = stage.get_meters_per_unit()
	assert_eq(mpu, 1.0, "Meters per unit should be 1.0")
	# RefCounted objects are auto-freed
