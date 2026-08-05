extends SceneTree

const EDITOR_TESTS := {
	"test_importer_variants.gd": true,
}

const EXPECTED_OUTPUT := {
	"test_source_aware_inbetween_blend_shape_warning.gd": "USD source-aware static save could not merge unsupported edit: derived inbetween blend-shape track at /root/Plane/Skel/Anim:Key_1__inbetween__HalfKey",
	"test_source_aware_static_material_rebind_warning.gd": "USD source-aware static save could not merge unsupported edit: material replacement/rebinding changed at /Root/Quad",
	"test_source_aware_static_material_subset_rebind_warning.gd": "USD source-aware static save could not merge unsupported edit: material subset rebinding changed at /Root/Panel surface 1",
	"test_source_aware_static_mesh_index_warning.gd": "USD source-aware static save could not merge unsupported edit: mesh index buffer changed at /Root/Quad surface 0",
	"test_source_aware_static_mesh_primvar_warning.gd": "USD source-aware static save could not merge unsupported edit: mesh UV primvar changed at /Root/Quad surface 0",
	"test_source_aware_static_mesh_unsupported_warning.gd": "USD source-aware static save could not merge unsupported edit: mesh topology changed at /Root/Quad",
	"test_source_aware_static_texture_network_warning.gd": "USD source-aware static save could not merge unsupported edit: texture-network material edit changed at /TexturedQuad surface 0",
}

const TESTS := [
	"test_array_mesh_save.gd",
	"test_blend_shape_basic.gd",
	"test_composition_inherits_roundtrip.gd",
	"test_composition_payload_roundtrip.gd",
	"test_composition_reference_roundtrip.gd",
	"test_composition_specializes_roundtrip.gd",
	"test_edge_subset_roundtrip.gd",
	"test_eight_weights.gd",
	"test_generated_texture_save.gd",
	"test_generic_face_subset_roundtrip.gd",
	"test_importer_variants.gd",
	"test_inherited_material_subset_roundtrip.gd",
	"test_load_only.gd",
	"test_multi_anim_blendshape.gd",
	"test_mesh_orientation_normals.gd",
	"test_named_material_subsets_roundtrip.gd",
	"test_point_subset_roundtrip.gd",
	"test_points_blend_shape_basic.gd",
	"test_preview_lighting.gd",
	"test_preview_surface_extended_load.gd",
	"test_preview_texture_channels.gd",
	"test_reset_stage_transform.gd",
	"test_shared_subset_material_reuse.gd",
	"test_simple_scene_save.gd",
	"test_skel_animation_sparsity.gd",
	"test_skeleton_basic.gd",
	"test_skeleton_roundtrip_save.gd",
	"test_source_aware_blend_shape_weight_save.gd",
	"test_source_aware_inbetween_blend_shape_warning.gd",
	"test_source_aware_skeleton_animation_save.gd",
	"test_source_aware_skeleton_transform_channels_save.gd",
	"test_source_aware_static_material_save.gd",
	"test_source_aware_static_material_rebind_warning.gd",
	"test_source_aware_static_material_subset_rebind_warning.gd",
	"test_source_aware_static_mesh_index_warning.gd",
	"test_source_aware_static_mesh_primvar_warning.gd",
	"test_source_aware_static_mesh_points_save.gd",
	"test_source_aware_static_mesh_unsupported_warning.gd",
	"test_source_aware_static_texture_network_warning.gd",
	"test_source_aware_static_transform_save.gd",
	"test_skinning_basic.gd",
	"test_skip_preview_lighting_save.gd",
	"test_sparse_material_subset_roundtrip.gd",
	"test_standard_material_save.gd",
	"test_subset_material_save.gd",
	"test_textured_preview_load.gd",
	"test_unmapped_roundtrip.gd",
	"test_variant_boundary_edit_save.gd",
	"test_variant_boundary_metadata.gd",
	"test_variant_flattened_save_report.gd",
	"test_variant_usd_layer_preserve.gd",
	"test_variant_usdc_preserve.gd",
	"test_variant_usdz_preserve.gd",
]

const DEFAULT_TIMEOUT_MSEC := 60000
const SLOW_TEST_TIMEOUTS := {
	"test_importer_variants.gd": 120000,
	"test_variant_usdz_preserve.gd": 120000,
}

func _initialize() -> void:
	call_deferred("_run_all")

func _shell_quote(value: String) -> String:
	return "'" + value.replace("'", "'\\''") + "'"

func _read_file(path: String) -> String:
	if not FileAccess.file_exists(path):
		return ""
	return FileAccess.get_file_as_string(path)

func _remove_file(path: String) -> void:
	if FileAccess.file_exists(path):
		DirAccess.remove_absolute(path)

func _run_test_process(executable: String, project_path: String, test_name: String, output_dir: String) -> Dictionary:
	var log_path := output_dir.path_join(test_name.get_basename() + ".log")
	var status_path := output_dir.path_join(test_name.get_basename() + ".status")
	_remove_file(log_path)
	_remove_file(status_path)

	var args := PackedStringArray(["--headless"])
	if EDITOR_TESTS.has(test_name):
		args.append("--editor")
	args.append("--path")
	args.append(project_path)
	args.append("--script")
	args.append("res://tests/%s" % test_name)

	var command_parts := PackedStringArray([_shell_quote(executable)])
	for arg in args:
		command_parts.append(_shell_quote(arg))
	var command := "%s > %s 2>&1; printf %%s $? > %s" % [
		" ".join(command_parts),
		_shell_quote(log_path),
		_shell_quote(status_path),
	]

	var pid := OS.create_process("/bin/sh", PackedStringArray(["-c", command]), false)
	if pid <= 0:
		return {
			"exit_code": -1,
			"timed_out": false,
			"output": "Failed to launch test process.",
		}

	var timeout_msec := int(SLOW_TEST_TIMEOUTS.get(test_name, DEFAULT_TIMEOUT_MSEC))
	var start_msec := Time.get_ticks_msec()
	while not FileAccess.file_exists(status_path):
		if Time.get_ticks_msec() - start_msec > timeout_msec:
			OS.kill(pid)
			return {
				"exit_code": -1,
				"timed_out": true,
				"output": _read_file(log_path),
			}
		await process_frame

	var status_text := _read_file(status_path).strip_edges()
	return {
		"exit_code": int(status_text) if status_text.is_valid_int() else -1,
		"timed_out": false,
		"output": _read_file(log_path),
	}

func _run_all() -> void:
	var executable := OS.get_executable_path()
	var project_path := ProjectSettings.globalize_path("res://")
	var output_dir := ProjectSettings.globalize_path("user://test_runner")
	DirAccess.make_dir_recursive_absolute(output_dir)
	var failures := 0

	print("Running %d Godot USD extension tests..." % TESTS.size())
	for test_name in TESTS:
		var result := await _run_test_process(executable, project_path, test_name, output_dir)
		var exit_code := int(result["exit_code"])
		var output := String(result["output"])
		if exit_code == 0 and not bool(result["timed_out"]):
			var expected_output := String(EXPECTED_OUTPUT.get(test_name, ""))
			if not expected_output.is_empty() and not output.contains(expected_output):
				failures += 1
				push_error("FAIL %s (missing expected output)" % test_name)
				printerr(output)
				continue
			print("PASS %s" % test_name)
			continue

		failures += 1
		if bool(result["timed_out"]):
			push_error("TIMEOUT %s" % test_name)
		else:
			push_error("FAIL %s (exit %d)" % [test_name, exit_code])
		printerr(output)

	if failures > 0:
		push_error("%d Godot USD extension tests failed." % failures)
		quit(1)
		return

	print("All Godot USD extension tests passed.")
	quit(0)
