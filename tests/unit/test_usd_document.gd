extends GutTest
## Tests for UsdDocument - the current import/export functionality


func test_usd_document_can_be_instantiated():
	var doc = UsdDocument.new()
	assert_not_null(doc, "UsdDocument should be instantiable")
	# RefCounted objects are auto-freed when out of scope


func test_usd_state_can_be_instantiated():
	var state = UsdState.new()
	assert_not_null(state, "UsdState should be instantiable")
	# RefCounted objects are auto-freed


func test_usd_state_default_values():
	var state = UsdState.new()
	# Check default values are sensible
	assert_typeof(state.copyright, TYPE_STRING)
	assert_typeof(state.bake_fps, TYPE_FLOAT)
	# RefCounted objects are auto-freed


func test_usd_state_copyright_property():
	var state = UsdState.new()
	state.copyright = "Test Copyright 2025"
	assert_eq(state.copyright, "Test Copyright 2025", "Copyright should be settable")
	# RefCounted objects are auto-freed


func test_usd_state_bake_fps_property():
	var state = UsdState.new()
	state.bake_fps = 30.0
	assert_eq(state.bake_fps, 30.0, "Bake FPS should be settable")
	# RefCounted objects are auto-freed


func test_usd_export_settings_can_be_instantiated():
	var settings = UsdExportSettings.new()
	assert_not_null(settings, "UsdExportSettings should be instantiable")
	# RefCounted objects are auto-freed


func test_file_extension_for_binary_format():
	var doc = UsdDocument.new()
	var ext = doc.get_file_extension_for_format(true)
	assert_eq(ext, "usdc", "Binary format should use .usdc extension")
	# RefCounted objects are auto-freed


func test_file_extension_for_ascii_format():
	var doc = UsdDocument.new()
	var ext = doc.get_file_extension_for_format(false)
	assert_eq(ext, "usda", "ASCII format should use .usda extension")
	# RefCounted objects are auto-freed
