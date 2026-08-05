extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var root := Node3D.new()
    root.name = "TextureRoot"

    var box := MeshInstance3D.new()
    box.name = "TextureBox"
    var box_mesh := BoxMesh.new()
    box.mesh = box_mesh

    var albedo_image := Image.create_empty(2, 2, false, Image.FORMAT_RGBA8)
    albedo_image.fill(Color(0.8, 0.8, 0.8, 0.4))
    var albedo_texture := ImageTexture.create_from_image(albedo_image)

    var packed_image := Image.create_empty(2, 2, false, Image.FORMAT_RGBA8)
    packed_image.fill(Color(0.6, 0.2, 0.9, 1.0))
    var packed_texture := ImageTexture.create_from_image(packed_image)

    var material := StandardMaterial3D.new()
    material.set_albedo(Color(0.7, 0.7, 0.7, 0.5))
    material.set_transparency(BaseMaterial3D.TRANSPARENCY_ALPHA)
    material.set_feature(BaseMaterial3D.FEATURE_CLEARCOAT, true)
    material.set_clearcoat(0.6)
    material.set_clearcoat_roughness(0.2)
    material.set_feature(BaseMaterial3D.FEATURE_AMBIENT_OCCLUSION, true)
    material.set_ao_texture_channel(BaseMaterial3D.TEXTURE_CHANNEL_BLUE)
    material.set_texture(BaseMaterial3D.TEXTURE_ALBEDO, albedo_texture)
    material.set_texture(BaseMaterial3D.TEXTURE_CLEARCOAT, packed_texture)
    material.set_texture(BaseMaterial3D.TEXTURE_AMBIENT_OCCLUSION, packed_texture)
    box.material_override = material

    root.add_child(box)
    box.owner = root

    var packed := PackedScene.new()
    if packed.pack(root) != OK:
        root.free()
        _fail("Failed to pack generated texture scene", old_value)
        return
    root.free()

    var save_path := "user://generated_texture_saved.usda"
    if ResourceSaver.save(packed, save_path) != OK:
        _fail("Failed to save generated texture USDA", old_value)
        return

    var saved_text := FileAccess.get_file_as_string(save_path)
    if not _require(saved_text.contains("inputs:diffuseColor.connect"), "Saved USDA is missing diffuseColor texture connection", old_value):
        return
    if not _require(saved_text.contains("inputs:opacity.connect"), "Saved USDA is missing opacity texture connection", old_value):
        return
    if not _require(saved_text.contains("inputs:clearcoat.connect"), "Saved USDA is missing clearcoat texture connection", old_value):
        return
    if not _require(saved_text.contains("inputs:clearcoatRoughness.connect"), "Saved USDA is missing clearcoatRoughness texture connection", old_value):
        return
    if not _require(saved_text.contains("inputs:occlusion.connect"), "Saved USDA is missing occlusion texture connection", old_value):
        return
    if not _require(saved_text.contains("generated_texture_saved_assets/material_albedotexture.png"), "Saved USDA is missing generated albedo asset path", old_value):
        return
    if not _require(saved_text.contains("generated_texture_saved_assets/material_clearcoattexture.png"), "Saved USDA is missing generated clearcoat asset path", old_value):
        return

    if not _require(FileAccess.file_exists("user://generated_texture_saved_assets/material_albedotexture.png"), "Generated albedo asset file is missing", old_value):
        return
    if not _require(FileAccess.file_exists("user://generated_texture_saved_assets/material_clearcoattexture.png"), "Generated clearcoat asset file is missing", old_value):
        return

    var reloaded := load(save_path)
    if reloaded == null or not (reloaded is PackedScene):
        _fail("Failed to reload generated texture USDA", old_value)
        return

    var imported_root := (reloaded as PackedScene).instantiate()
    get_root().add_child(imported_root)

    var imported_box := _find_prim_node(imported_root, "/TextureRoot/TextureBox") as MeshInstance3D
    if imported_box == null:
        _fail("Failed to find /TextureRoot/TextureBox after reload", old_value)
        return

    var loaded_material := imported_box.get_active_material(0) as BaseMaterial3D
    if loaded_material == null:
        _fail("Reloaded generated-texture material is missing", old_value)
        return

    if not _require(loaded_material.get_texture(BaseMaterial3D.TEXTURE_ALBEDO) != null, "Reloaded albedo texture is missing", old_value):
        return
    if not _require(loaded_material.get_texture(BaseMaterial3D.TEXTURE_CLEARCOAT) != null, "Reloaded clearcoat texture is missing", old_value):
        return
    if not _require(loaded_material.get_texture(BaseMaterial3D.TEXTURE_AMBIENT_OCCLUSION) != null, "Reloaded AO texture is missing", old_value):
        return
    if not _require(loaded_material.get_transparency() == BaseMaterial3D.TRANSPARENCY_ALPHA, "Reloaded transparency mode mismatch", old_value):
        return

    print("Generated texture USDA save verified.")

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
