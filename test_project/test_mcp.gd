extends SceneTree

func _init():
	print("=== Testing Godot with MCP flag ===")

	# Print version information from Engine
	var version = Engine.get_version_info()
	print("Godot Version: %d.%d.%d" % [version.major, version.minor, version.patch])

	# Check command line args
	var args = OS.get_cmdline_args()
	print("Command line args:", args)

	var has_mcp = false
	for arg in args:
		if arg == "--mcp" or arg == "--interactive":
			has_mcp = true
			break

	if has_mcp:
		print("✓ MCP flag detected - server should be running")
	else:
		print("✗ MCP flag not detected")

	print("=== Test completed ===")
	print("=== Waiting for stdin input for MCP... (Ctrl+C to quit) ===")

	# Don't quit - let the MCP server run
	# quit()
