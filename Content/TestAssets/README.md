# Test Assets

This directory contains test assets used for validating the UnrealMCPServer plugin functionality.

## Purpose

These assets are included with the plugin to provide reliable test targets that are always available when the plugin is installed. This ensures tests can validate resource reading, prompt generation, and other functionality without depending on project-specific assets.

## Asset Paths

All test assets are located under the plugin's content path:
- Plugin content path: `/UnrealMCPServer/TestAssets/...`

## Test Assets

The following test assets are available in `Content/TestAssets/`:

### Blueprints

1. **`MCP_TestPawn`** - A Blueprint Pawn for testing
   - Path: `/UnrealMCPServer/TestAssets/MCP_TestPawn`
   - Purpose: Blueprint resource reading tests (T3D and Markdown export)
   - Type: Blueprint (Pawn class)

### Non-Blueprint Assets

1. **`MCP_TestMaterial`** - A Material asset for testing
   - Path: `/UnrealMCPServer/TestAssets/MCP_TestMaterial`
   - Purpose: Non-Blueprint asset testing (query, export, etc.)
   - Type: Material

## Creating Test Assets

### Option 1: Create in Unreal Editor

1. Open Unreal Editor with a project that has the UnrealMCPServer plugin enabled
2. Navigate to the Content Browser
3. Right-click in `Plugins/UnrealMCPServer/Content/TestAssets/Blueprints/`
4. Create Blueprint classes as needed
5. Save the assets

### Option 2: Copy from Project

If you have test Blueprints in your project, you can copy them to this directory.

## Usage in Tests

Tests should reference these assets using their plugin paths:

```python
# Test asset paths (defined in UnrealMCPProxy/tests/test_assets.py)
TEST_BLUEPRINT_PAWN = "/UnrealMCPServer/TestAssets/MCP_TestPawn"
TEST_MATERIAL = "/UnrealMCPServer/TestAssets/MCP_TestMaterial"
```

## Notes

- These assets are part of the plugin, so they will be available in any project that has the plugin installed
- Asset paths use the plugin content prefix `/UnrealMCPServer/` instead of `/Game/`
- The `.uasset` files are binary and must be created in Unreal Editor
- This directory structure is a placeholder - actual Blueprint assets need to be created in the editor

