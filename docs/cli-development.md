# CLI Development Workflow for USD-Godot

This document describes how to develop and test USD-Godot bindings using the command line, avoiding the slow GUI iteration cycle.

## Prerequisites

- Godot 4.2+ installed and accessible via `godot` command or known path
- The GUT test framework (already installed in `addons/gut/`)
- Built `libgodot-usd` library in `lib/`

## Quick Start

```bash
# Run all tests
./run_tests.sh

# Run a specific test file
./run_tests.sh test_usd_document

# Run with verbose output
./run_tests.sh -v
```

## Test Structure

```
tests/
├── unit/                          # Unit tests
│   ├── test_usd_document.gd       # Tests for existing UsdDocument
│   ├── test_usd_import.gd         # Tests for import functionality
│   ├── test_usd_stage_proxy.gd    # Tests for UsdStageProxy (TDD stubs)
│   └── test_usd_prim_proxy.gd     # Tests for UsdPrimProxy (TDD stubs)
├── integration/                   # Integration tests (future)
└── fixtures/                      # Test USD files
    ├── simple_cube.usda
    └── hierarchy.usda
```

## Writing Tests

### Basic Test Structure

```gdscript
extends GutTest

func test_something():
    var doc = UsdDocument.new()
    assert_not_null(doc)
    # Note: UsdDocument, UsdStageProxy, UsdPrimProxy are all RefCounted
    # They auto-free when no longer referenced - never call .free() on them

func test_pending_feature():
    pending("Feature not yet implemented")
    # Commented test code here
```

### Using Fixtures

```gdscript
const FIXTURES_PATH = "res://tests/fixtures/"

func test_import():
    var doc = UsdDocument.new()
    var state = UsdState.new()
    var parent = Node3D.new()
    add_child_autofree(parent)  # Auto-cleanup

    var err = doc.import_from_file(FIXTURES_PATH + "simple_cube.usda", parent, state)
    assert_eq(err, OK)
```

## Manual CLI Testing

### Run a Script Directly

Create a test script:

```gdscript
# test_manual.gd
extends SceneTree

func _init():
    print("Testing USD bindings...")

    var doc = UsdDocument.new()
    var state = UsdState.new()

    print("UsdDocument created: ", doc)
    print("Extension for ASCII: ", doc.get_file_extension_for_format(false))

    quit()
```

Run it:

```bash
godot --headless -s test_manual.gd
```

### Interactive REPL-like Testing

For quick experiments, create a script that waits for input:

```gdscript
# repl.gd
extends SceneTree

func _init():
    print("USD-Godot REPL ready")
    # Your test code here

    # Don't quit immediately
    await get_tree().create_timer(3600).timeout
```

Run and Ctrl+C when done:

```bash
godot --headless -s repl.gd
```

## GUT CLI Options

Full GUT command:

```bash
godot --headless -d -s addons/gut/gut_cmdln.gd [options]
```

Common options:

| Option | Description |
|--------|-------------|
| `-gtest=<path>` | Run specific test file |
| `-gdir=<path>` | Test directory |
| `-ginner_class=<name>` | Run specific inner class |
| `-gunit_test_name=<name>` | Run specific test method |
| `-glog=<0-3>` | Log level (0=errors, 3=verbose) |
| `-gexit` | Exit after tests |
| `-gexit_on_success` | Exit only if all pass |

### Examples

```bash
# Run only UsdDocument tests
godot --headless -d -s addons/gut/gut_cmdln.gd -gtest=res://tests/unit/test_usd_document.gd

# Run a specific test method
godot --headless -d -s addons/gut/gut_cmdln.gd -gunit_test_name=test_file_extension_for_binary_format

# Verbose output
godot --headless -d -s addons/gut/gut_cmdln.gd -glog=3
```

## TDD Workflow for New Bindings

1. **Write failing tests first** in `tests/unit/test_usd_*_proxy.gd`
   - Tests use `pending()` until implementation begins
   - Uncomment test code when implementing

2. **Implement C++ class**
   - Add to `src/usd_*_proxy.cpp`
   - Register in `register_types.cpp`

3. **Build**
   ```bash
   ./build.sh
   ```

4. **Run tests**
   ```bash
   ./run_tests.sh test_usd_stage_proxy
   ```

5. **Iterate until green**

## Debugging Tips

### USD TF_DEBUG Environment Variables

USD has a powerful built-in debugging system using TF_DEBUG environment variables. These are invaluable for diagnosing plugin loading and other issues:

```bash
# Debug plugin registration and discovery
TF_DEBUG="PLUG_REGISTRATION,PLUG_INFO_SEARCH" ./run_tests.sh

# See all available TF_DEBUG flags
TF_DEBUG="PLUG_*" ./run_tests.sh 2>&1 | head -50
```

Common TF_DEBUG flags for this plugin:
- `PLUG_REGISTRATION` - Shows which plugins are being registered
- `PLUG_INFO_SEARCH` - Shows where USD is looking for plugInfo.json files
- `AR_DEFAULT_RESOLVER` - Asset resolver debugging
- `SDF_LAYER` - Layer loading/saving debugging
- `USD_STAGE_OPEN` - Stage opening debugging

### Print Debug Output

```gdscript
func test_with_debug():
    var doc = UsdDocument.new()
    gut.p("Document created: %s" % doc)
    gut.p("Methods: %s" % doc.get_method_list())
    # Note: UsdDocument is RefCounted, no need to call .free()
```

### Check Class Registration

```gdscript
func test_class_exists():
    assert_true(ClassDB.class_exists("UsdDocument"))
    assert_true(ClassDB.class_exists("UsdState"))

    # Check methods
    var methods = ClassDB.class_get_method_list("UsdDocument")
    gut.p("UsdDocument methods: %s" % methods)
```

### Inspect Singleton

```gdscript
func test_singleton():
    if Engine.has_singleton("USDPlugin"):
        var plugin = Engine.get_singleton("USDPlugin")
        gut.p("Plugin: %s" % plugin)
```

## CI Integration

Add to your CI pipeline:

```yaml
# .github/workflows/test.yml
test:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - name: Setup Godot
      uses: chickensoft-games/setup-godot@v1
      with:
        version: 4.2.1
    - name: Build Extension
      run: ./build.sh
    - name: Run Tests
      run: |
        godot --headless -d -s addons/gut/gut_cmdln.gd --quit-on-finish
```

## Performance Comparison

| Method | Cold Start | Iteration |
|--------|------------|-----------|
| GUI Editor | ~5-10s | ~2-5s per change |
| CLI `run_tests.sh` | ~2-3s | ~1-2s per change |
| Specific test file | ~1-2s | ~0.5-1s per change |

The CLI approach is **3-5x faster** for iterative development.
