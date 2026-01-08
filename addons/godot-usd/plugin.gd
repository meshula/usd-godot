@tool
extends EditorPlugin

var plugin_instance = null

func _enter_tree():
	# Initialize the plugin
	print("Godot-USD Plugin: GDScript _enter_tree called")
	
	# Check if the GDExtension plugin is available
	if Engine.has_singleton("USDPlugin"):
		plugin_instance = Engine.get_singleton("USDPlugin")
		print("Godot-USD Plugin: GDExtension plugin loaded successfully")
	else:
		push_error("Godot-USD Plugin: Failed to load GDExtension plugin")

func _exit_tree():
	# Clean up the plugin
	print("Godot-USD Plugin: GDScript _exit_tree called")
	plugin_instance = null

func _has_main_screen():
	# Return true if the plugin needs a main screen
	return false

func _get_plugin_name():
	# Return the name of the plugin
	return "USD"
