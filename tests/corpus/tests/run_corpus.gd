extends SceneTree
#
# Fidelity-corpus scoreboard harness for usd-godot.
#
# The test scripts in this directory were harvested verbatim from godot-usd
# (MIT, (c) 2026 Miguel de Icaza). They are behavior-level oracles that define
# what a faithful USD <-> Godot translation should do. usd-godot does not yet
# implement most of that surface, so most tests are EXPECTED to fail today.
#
# Rather than hard-fail like run_all.gd (kept verbatim as the reference oracle),
# this harness produces a moving SCOREBOARD:
#
#   PASS            test passed and is in MUST_PASS            -> good
#   XFAIL           test failed and is NOT in MUST_PASS        -> expected (not implemented yet)
#   UNEXPECTED-PASS test passed but is NOT in MUST_PASS        -> a feature landed; promote it!
#   REGRESSION      test failed but IS in MUST_PASS            -> broke something -> exit 1
#
# Workflow: as each transplant phase lands a feature, move the now-passing test
# names into MUST_PASS below. CI fails only on REGRESSION, so the corpus can be
# committed and tracked from day one while the "N/53 passing" number climbs.
#
# Run via tests/corpus/run_corpus.sh (stages the built extension first).

# Tests that MUST pass. Empty at Phase 0 baseline (nothing transplanted yet).
# Add names here as features land (Phase 2: materials; Phase 3: skel/curves/lights; Phase 4: save).
const MUST_PASS := {
	# Phase 1 (ResourceFormatLoader + UsdSceneBuilder). Verified substantive, not hollow:
	"test_load_only.gd": true,                # 2 assertion sites: load() -> PackedScene
	"test_mesh_orientation_normals.gd": true, # 13 assertion sites: winding + face-varying normals
	# Phase 2 (materials + textures + usdz). High assertion counts confirm real validation:
	"test_preview_surface_extended_load.gd": true, # 22 sites: UsdPreviewSurface scalar/color inputs
	"test_preview_texture_channels.gd": true,      # 21 sites: UsdUVTexture channel mapping
	"test_textured_preview_load.gd": true,         # 7 sites: textured + usdz-packaged albedo
	# Phase 3 (skel): skinning verified substantive:
	"test_skinning_basic.gd": true,                # 30 sites: Skeleton3D + Skin + skeleton_path + bone/weight arrays
	"test_eight_weights.gd": true,                 # 17 sites: 8-influence skinning
	"test_blend_shape_basic.gd": true,             # 33 sites: blend shapes + inbetweens + anim tracks
	"test_points_blend_shape_basic.gd": true,      # 20 sites: point-based blend shapes
	"test_multi_anim_blendshape.gd": true,         # 12 sites: multiple animation clips
	"test_skel_animation_sparsity.gd": true,       # 27 sites: sparse joint animation tracks
	"test_reset_stage_transform.gd": true,         # 2 sites: stage up-axis/scale correction reset
	# Phase 4 (saver) — authored-scene export path:
	"test_simple_scene_save.gd": true,             # authored Node3D+primitive tree -> USDA round-trip
	"test_array_mesh_save.gd": true,               # generic ArrayMesh + material -> USDA round-trip
	"test_standard_material_save.gd": true,        # full UsdPreviewSurface (specular/clearcoat/opacity) round-trip
	"test_generated_texture_save.gd": true,        # UsdUVTexture (generated texture) export + reload
	"test_subset_material_save.gd": true,          # multi-surface -> GeomSubset export + per-subset materials
	# Phase 4c (source-preserving save): byte-copy source + author only changed transforms.
	"test_source_aware_static_transform_save.gd": true, # transform edit merged into copied source layer
	"test_named_material_subsets_roundtrip.gd": true,   # authored subset names/families preserved
	"test_generic_face_subset_roundtrip.gd": true,
	"test_edge_subset_roundtrip.gd": true,
	"test_point_subset_roundtrip.gd": true,
	"test_inherited_material_subset_roundtrip.gd": true,
	"test_sparse_material_subset_roundtrip.gd": true,
	"test_skeleton_roundtrip_save.gd": true,            # skeleton/skin structure preserved through round-trip
	# NOT promoted (hollow, print-only — pass as soon as load() works, assert nothing):
	#   test_preview_lighting.gd, test_skeleton_basic.gd  -> promote when lights/skel are real.
}

# Tests that require an editor build (--editor), not just --headless.
const EDITOR_TESTS := {
	"test_importer_variants.gd": true,
}

# Oracle assertions: these tests only count as PASS if they exit 0 AND their
# output contains the exact expected warning substring (copied from run_all.gd).
const EXPECTED_OUTPUT := {
	"test_source_aware_inbetween_blend_shape_warning.gd": "USD source-aware static save could not merge unsupported edit: derived inbetween blend-shape track at /root/Plane/Skel/Anim:Key_1__inbetween__HalfKey",
	"test_source_aware_static_material_rebind_warning.gd": "USD source-aware static save could not merge unsupported edit: material replacement/rebinding changed at /Root/Quad",
	"test_source_aware_static_material_subset_rebind_warning.gd": "USD source-aware static save could not merge unsupported edit: material subset rebinding changed at /Root/Panel surface 1",
	"test_source_aware_static_mesh_index_warning.gd": "USD source-aware static save could not merge unsupported edit: mesh index buffer changed at /Root/Quad surface 0",
	"test_source_aware_static_mesh_primvar_warning.gd": "USD source-aware static save could not merge unsupported edit: mesh UV primvar changed at /Root/Quad surface 0",
	"test_source_aware_static_mesh_unsupported_warning.gd": "USD source-aware static save could not merge unsupported edit: mesh topology changed at /Root/Quad",
	"test_source_aware_static_texture_network_warning.gd": "USD source-aware static save could not merge unsupported edit: texture-network material edit changed at /TexturedQuad surface 0",
}

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

func _discover_tests() -> Array:
	# Every test_*.gd in this directory is part of the corpus. Discovering them
	# keeps this harness in sync with the ported scripts (no hand-maintained list).
	var names := []
	var dir := DirAccess.open("res://tests")
	if dir == null:
		push_error("Cannot open res://tests")
		return names
	dir.list_dir_begin()
	var entry := dir.get_next()
	while entry != "":
		if not dir.current_is_dir() and entry.begins_with("test_") and entry.ends_with(".gd"):
			names.append(entry)
		entry = dir.get_next()
	dir.list_dir_end()
	names.sort()
	return names

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

	var start_msec := Time.get_ticks_msec()
	var pid := OS.create_process("/bin/sh", PackedStringArray(["-c", command]), false)
	if pid <= 0:
		return {"exit_code": -1, "timed_out": false, "output": "Failed to launch test process.", "duration_ms": 0, "log_path": log_path}

	var timeout_msec := int(SLOW_TEST_TIMEOUTS.get(test_name, DEFAULT_TIMEOUT_MSEC))
	while not FileAccess.file_exists(status_path):
		if Time.get_ticks_msec() - start_msec > timeout_msec:
			OS.kill(pid)
			return {"exit_code": -1, "timed_out": true, "output": _read_file(log_path), "duration_ms": Time.get_ticks_msec() - start_msec, "log_path": log_path}
		await process_frame

	var status_text := _read_file(status_path).strip_edges()
	return {
		"exit_code": int(status_text) if status_text.is_valid_int() else -1,
		"timed_out": false,
		"output": _read_file(log_path),
		"duration_ms": Time.get_ticks_msec() - start_msec,
		"log_path": log_path,
	}

# Markers proving the test did NOT actually execute to completion. Godot's --script path exits 0
# even when a script fails to COMPILE (e.g. references a class usd-godot doesn't define yet, like
# UsdStageInstance), so exit-0 alone is not proof of a pass. A crashed engine must also not count.
const FAILURE_MARKERS := [
	"Failed to load script",   # parse/compile error -> script never ran
	"Program crashed",         # engine segfault
]

# A pass slower than this is flagged for review (a hollow "pass" or a hang often runs long).
const SLOW_MS := 8000

# Log lines that are benign noise, not real errors, when deciding if a pass is "clean".
const BENIGN_ERROR_SUBSTRINGS := [
	"Failed to open 'user://logs",   # headless has no writable log dir; harmless
]

# Forensics: count error/warning lines in a test's output. A genuine pass should be clean; a pass
# that still emitted errors is SUSPECT (e.g. a print-only diagnostic test that "passes" by exiting 0
# without actually validating the feature, or a runtime error swallowed before quit(0)).
func _log_diagnostics(output: String) -> Dictionary:
	var errors := 0
	var warnings := 0
	for line in output.split("\n", false):
		var is_error := line.contains("ERROR:") or line.contains("SCRIPT ERROR")
		if is_error:
			var benign := false
			for b in BENIGN_ERROR_SUBSTRINGS:
				if line.contains(b):
					benign = true
					break
			if not benign:
				errors += 1
		elif line.contains("WARNING:") or line.contains("USD:"):
			warnings += 1
	return {"errors": errors, "warnings": warnings}

# Count failure/assertion sites in a test's source. This is an ADVISORY signal, not a verdict:
# a print-only diagnostic guards only the initial load() (push_error + quit = ~2 sites) then quits 0
# without asserting its feature, which looks identical to a legitimately minimal test like
# test_load_only. So a low count means "inspect before trusting", while a high count (many geometry/
# skel/material assertions) means the pass genuinely exercises something. Not used to auto-fail.
const SUBSTANTIVE_SITES := 5

func _assertion_sites(test_name: String) -> int:
	var src := _read_file("res://tests/%s" % test_name)
	var count := 0
	for line in src.split("\n", false):
		if line.contains("quit(1)") or line.contains("quit(2)") or line.contains("_fail(") \
				or line.contains("_require(") or line.contains("push_error("):
			count += 1
	return count

# Returns true if the test met its pass criteria (ran to completion, exit 0, + any required substring).
func _test_passed(test_name: String, result: Dictionary) -> bool:
	if bool(result["timed_out"]):
		return false
	if int(result["exit_code"]) != 0:
		return false
	var output := String(result["output"])
	for marker in FAILURE_MARKERS:
		if output.contains(marker):
			return false
	var expected_output := String(EXPECTED_OUTPUT.get(test_name, ""))
	if not expected_output.is_empty() and not output.contains(expected_output):
		return false
	return true

func _run_all() -> void:
	var executable := OS.get_executable_path()
	var project_path := ProjectSettings.globalize_path("res://")
	var output_dir := ProjectSettings.globalize_path("user://corpus_runner")
	DirAccess.make_dir_recursive_absolute(output_dir)

	var tests := _discover_tests()
	var passed := []
	var xfailed := []
	var unexpected_pass := []
	var regressions := []
	var records := {}          # test_name -> {category, duration_ms, errors, warnings, suspect, log_path}
	var suite_start := Time.get_ticks_msec()

	print("=== usd-godot fidelity corpus: %d tests ===" % tests.size())
	for test_name in tests:
		var result := await _run_test_process(executable, project_path, test_name, output_dir)
		var ok := _test_passed(test_name, result)
		var required := MUST_PASS.has(test_name)
		var diag := _log_diagnostics(String(result["output"]))
		var duration_ms := int(result.get("duration_ms", 0))

		var category := ""
		if ok and required:
			category = "PASS"
			passed.append(test_name)
		elif ok and not required:
			category = "UNEXPECTED-PASS"
			unexpected_pass.append(test_name)
		elif not ok and not required:
			category = "XFAIL"
			xfailed.append(test_name)
		else:
			category = "REGRESSION"
			regressions.append(test_name)

		# SUSPECT = a pass that emitted (non-benign) errors or ran slow (hang / hollow-heavy work).
		# Assertion count is reported as advisory (see below), NOT used to auto-fail.
		var is_pass := category == "PASS" or category == "UNEXPECTED-PASS"
		var sites := _assertion_sites(test_name)
		var low_assert := is_pass and sites < SUBSTANTIVE_SITES
		var suspect := is_pass and (int(diag["errors"]) > 0 or duration_ms >= SLOW_MS)
		records[test_name] = {
			"category": category, "duration_ms": duration_ms,
			"errors": int(diag["errors"]), "warnings": int(diag["warnings"]),
			"suspect": suspect, "low_assert": low_assert, "sites": sites,
			"log_path": String(result.get("log_path", "")),
			"timed_out": bool(result["timed_out"]),
		}

		var flags := ""
		if suspect:
			flags += "  ⚠ SUSPECT"
		if duration_ms >= SLOW_MS:
			flags += "  [SLOW]"
		if int(diag["errors"]) > 0:
			flags += "  [%d err]" % int(diag["errors"])
		var line := "  %-15s %-52s %6d ms%s" % [category, test_name, duration_ms, flags]
		if category == "REGRESSION":
			push_error(line)
			printerr(String(result["output"]))
		else:
			print(line)

	var implemented := passed.size() + unexpected_pass.size()
	var suite_ms := Time.get_ticks_msec() - suite_start
	print("")
	print("=== scoreboard ===")
	print("  passing (fidelity):   %d / %d" % [implemented, tests.size()])
	print("  PASS (required):      %d" % passed.size())
	print("  UNEXPECTED-PASS:      %d" % unexpected_pass.size())
	print("  XFAIL (not yet impl): %d" % xfailed.size())
	print("  REGRESSION:           %d" % regressions.size())
	print("  wall-clock:           %d ms" % suite_ms)

	# --- forensics: flag passes that need scrutiny before promotion --------------------------
	var suspects := []
	for t in tests:
		if records[t]["suspect"]:
			suspects.append(t)
	if not suspects.is_empty():
		print("")
		print("⚠ SUSPECT passes (%d) — exited 0 but emitted errors or ran slow; verify they actually" % suspects.size())
		print("  validate the feature (many godot-usd tests print-only and 'pass' hollow) BEFORE promoting:")
		for t in suspects:
			var r = records[t]
			print("  - %-52s %6d ms  errors=%d  log=%s" % [t, r["duration_ms"], r["errors"], r["log_path"]])

	if not unexpected_pass.is_empty():
		print("")
		print("NOTE: %d newly-passing test(s) not in MUST_PASS." % unexpected_pass.size())
		print("  A 'pass' only means the script exited 0. Several corpus tests are PRINT-ONLY")
		print("  diagnostics that pass hollow as soon as load() works, without asserting their")
		print("  feature. Inspect each log and promote into MUST_PASS only those that show real")
		print("  evidence of the feature (assertions, expected node types, bone/track counts):")
		for t in unexpected_pass:
			var r = records[t]
			var tag := "  assertions=%d" % r["sites"]
			if r["low_assert"]:
				tag += " (LOW — verify it isn't print-only)"
			if r["suspect"]:
				tag += "  ⚠ SUSPECT"
			print("  - %-46s %6d ms%s" % [t, r["duration_ms"], tag])
			print("      log: %s" % r["log_path"])

	if not regressions.is_empty():
		push_error("%d corpus regression(s)." % regressions.size())
		quit(1)
		return
	quit(0)
