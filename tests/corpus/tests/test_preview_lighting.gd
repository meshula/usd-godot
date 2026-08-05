extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var args := OS.get_cmdline_user_args()
    var path := "res://samples/basic.usda"
    var mode := 1
    if args.size() >= 1:
        path = args[0]
    if args.size() >= 2:
        mode = int(args[1])

    var project_settings := ProjectSettings
    var old_value = project_settings.get_setting(SETTING, 1)
    project_settings.set_setting(SETTING, mode)

    var packed := load(path)
    if packed == null or not (packed is PackedScene):
        push_error("Failed to load PackedScene for %s" % path)
        project_settings.set_setting(SETTING, old_value)
        quit(1)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)

    print("Mode: %d" % mode)
    print("Root class: %s" % root.get_class())
    print("Root children: %d" % root.get_child_count())
    if root.get_child_count() > 0:
        var generated := root.get_child(0)
        print("Generated children: %d" % generated.get_child_count())
        if generated.has_meta("usd"):
            print("Generated USD meta: %s" % generated.get_meta("usd"))
        for child in generated.get_children():
            print("Child: %s (%s)" % [child.name, child.get_class()])
            if child.has_meta("usd"):
                print("Child USD meta: %s" % child.get_meta("usd"))
            if child is WorldEnvironment:
                var env := (child as WorldEnvironment).environment
                print("Environment background: %s" % env.get_background())
            elif child is DirectionalLight3D:
                print("Directional shadow: %s" % (child as DirectionalLight3D).has_shadow())

    root.free()
    project_settings.set_setting(SETTING, old_value)
    await process_frame
    quit(0)
