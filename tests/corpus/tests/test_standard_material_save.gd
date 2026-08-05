extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var root := Node3D.new()
    root.name = "MaterialRoot"

    var box := MeshInstance3D.new()
    box.name = "MaterialBox"
    var box_mesh := BoxMesh.new()
    box_mesh.size = Vector3(2.0, 2.0, 2.0)
    box.mesh = box_mesh

    var material := StandardMaterial3D.new()
    material.set_albedo(Color(0.8, 0.2, 0.1, 0.5))
    material.set_metallic(0.35)
    material.set_roughness(0.65)
    material.set_transparency(BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR)
    material.set_alpha_scissor_threshold(0.4)
    material.set_feature(BaseMaterial3D.FEATURE_EMISSION, true)
    material.set_emission(Color(0.15, 0.05, 0.02))
    material.set_feature(BaseMaterial3D.FEATURE_CLEARCOAT, true)
    material.set_clearcoat(0.6)
    material.set_clearcoat_roughness(0.2)
    material.set_meta("usd", {
        "usd:preview_surface_use_specular_workflow": true,
        "usd:preview_surface_ior": 1.5,
        "usd:preview_surface_specular_color": Color(0.04, 0.04, 0.04, 1.0),
    })
    box.material_override = material

    root.add_child(box)
    box.owner = root

    var packed := PackedScene.new()
    if packed.pack(root) != OK:
        root.free()
        _fail("Failed to pack material test scene", old_value)
        return
    root.free()

    var save_path := "user://standard_material_saved.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save standard material USDA", old_value)
        return

    var saved_text := FileAccess.get_file_as_string(save_path)
    if not _require(saved_text.contains("UsdPreviewSurface"), "Saved USDA is missing UsdPreviewSurface", old_value):
        return
    if not _require(saved_text.contains("inputs:useSpecularWorkflow"), "Saved USDA is missing useSpecularWorkflow", old_value):
        return
    if not _require(saved_text.contains("inputs:specularColor"), "Saved USDA is missing specularColor", old_value):
        return
    if not _require(saved_text.contains("inputs:opacityThreshold = 0.4"), "Saved USDA is missing opacityThreshold", old_value):
        return
    if not _require(saved_text.contains("inputs:clearcoat = 0.6"), "Saved USDA is missing clearcoat", old_value):
        return
    if not _require(saved_text.contains("rel material:binding"), "Saved USDA is missing material binding", old_value):
        return

    var reloaded := load(save_path)
    if reloaded == null or not (reloaded is PackedScene):
        _fail("Failed to reload standard material USDA", old_value)
        return

    var imported_root := (reloaded as PackedScene).instantiate()
    get_root().add_child(imported_root)

    var imported_box := _find_prim_node(imported_root, "/MaterialRoot/MaterialBox") as MeshInstance3D
    if imported_box == null:
        _fail("Failed to find /MaterialRoot/MaterialBox after reload", old_value)
        return

    var loaded_material := imported_box.get_active_material(0) as BaseMaterial3D
    if loaded_material == null:
        _fail("Reloaded primitive material is missing", old_value)
        return

    var loaded_albedo := loaded_material.get_albedo()
    if not _require(is_equal_approx(loaded_albedo.r, 0.8), "Reloaded albedo.r mismatch", old_value):
        return
    if not _require(is_equal_approx(loaded_albedo.g, 0.2), "Reloaded albedo.g mismatch", old_value):
        return
    if not _require(is_equal_approx(loaded_albedo.b, 0.1), "Reloaded albedo.b mismatch", old_value):
        return
    if not _require(is_equal_approx(loaded_albedo.a, 0.5), "Reloaded albedo.a mismatch", old_value):
        return
    if not _require(is_equal_approx(loaded_material.get_metallic(), 0.0), "Reloaded metallic should follow specular workflow path", old_value):
        return
    if not _require(is_equal_approx(loaded_material.get_roughness(), 0.65), "Reloaded roughness mismatch", old_value):
        return
    if not _require(loaded_material.get_feature(BaseMaterial3D.FEATURE_EMISSION), "Reloaded emission flag is missing", old_value):
        return
    if not _require(is_equal_approx(loaded_material.get_emission().r, 0.15), "Reloaded emission.r mismatch", old_value):
        return
    if not _require(loaded_material.get_transparency() == BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR, "Reloaded transparency mode mismatch", old_value):
        return
    if not _require(is_equal_approx(loaded_material.get_alpha_scissor_threshold(), 0.4), "Reloaded alpha scissor mismatch", old_value):
        return
    if not _require(loaded_material.get_feature(BaseMaterial3D.FEATURE_CLEARCOAT), "Reloaded clearcoat flag is missing", old_value):
        return
    if not _require(is_equal_approx(loaded_material.get_clearcoat(), 0.6), "Reloaded clearcoat mismatch", old_value):
        return
    if not _require(is_equal_approx(loaded_material.get_clearcoat_roughness(), 0.2), "Reloaded clearcoat roughness mismatch", old_value):
        return

    var loaded_meta := loaded_material.get_meta("usd", {}) as Dictionary
    if not _require(bool(loaded_meta.get("usd:preview_surface_use_specular_workflow", false)), "Reloaded material metadata lost useSpecularWorkflow", old_value):
        return
    if not _require(is_equal_approx(float(loaded_meta.get("usd:preview_surface_ior", 0.0)), 1.5), "Reloaded material metadata lost ior", old_value):
        return

    print("StandardMaterial3D USDA round-trip verified.")

    imported_root.free()
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

func _require(condition: bool, message: String, old_value: Variant) -> bool:
    if condition:
        return true
    _fail(message, old_value)
    return false

func _fail(message: String, old_value: Variant) -> void:
    push_error(message)
    ProjectSettings.set_setting(SETTING, old_value)
    quit(1)
