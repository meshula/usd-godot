extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/skeleton_basic.usda")
    if packed == null or not (packed is PackedScene):
        push_error("Failed to load skeleton_basic.usda")
        ProjectSettings.set_setting(SETTING, old_value)
        quit(1)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)

    var generated := root.get_child(0)
    print("Generated children: %d" % generated.get_child_count())
    for child in _walk(generated):
        print("Child: %s (%s)" % [child.name, child.get_class()])
        if child is Skeleton3D:
            var skeleton := child as Skeleton3D
            print("Bone count: %d" % skeleton.get_bone_count())
            for bone_index in skeleton.get_bone_count():
                print("Bone %d: %s parent=%d rest=%s" % [bone_index, skeleton.get_bone_name(bone_index), skeleton.get_bone_parent(bone_index), skeleton.get_bone_rest(bone_index)])
                if skeleton.has_bone_meta(bone_index, &"usd_joint_path"):
                    print("Bone meta path: %s" % skeleton.get_bone_meta(bone_index, &"usd_joint_path"))
                if skeleton.has_bone_meta(bone_index, &"usd_joint_parent_path"):
                    print("Bone meta parent: %s" % skeleton.get_bone_meta(bone_index, &"usd_joint_parent_path"))
            if skeleton.has_meta("usd"):
                print("Skeleton USD meta: %s" % skeleton.get_meta("usd"))
        elif child is AnimationPlayer:
            var player := child as AnimationPlayer
            print("Animation player root: %s" % player.get_root())
            print("Animations: %s" % player.get_animation_list())
            if player.has_animation(&"Anim1"):
                var animation := player.get_animation(&"Anim1")
                print("Anim1 length: %s" % animation.get_length())
                print("Anim1 tracks: %d" % animation.get_track_count())
                if animation.get_track_count() > 0:
                    print("Track 0 path: %s" % animation.track_get_path(0))
                    print("Track 0 type: %s" % animation.track_get_type(0))
                    print("Track 0 keys: %d" % animation.track_get_key_count(0))

    root.free()
    ProjectSettings.set_setting(SETTING, old_value)
    await process_frame
    quit(0)

func _walk(node: Node) -> Array[Node]:
    var result: Array[Node] = []
    for child in node.get_children():
        result.append(child)
        result.append_array(_walk(child))
    return result
