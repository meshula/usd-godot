extends SceneTree

func _initialize() -> void:
    call_deferred("_run_test")

func _run_test() -> void:
    var path := "res://samples/cube.usda"
    var args := OS.get_cmdline_user_args()
    if not args.is_empty():
        path = args[0]

    var packed = load(path)
    if packed == null:
        push_error("Failed to load USD scene: %s" % path)
        quit(1)
        return

    if not (packed is PackedScene):
        push_error("Loaded resource is not a PackedScene: %s" % [packed])
        quit(2)
        return

    print("Loaded scene resource: %s" % packed.get_class())
    packed = null
    await process_frame
    quit(0)
