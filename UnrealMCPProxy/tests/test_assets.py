"""Test asset paths for UnrealMCPServer plugin.

These are plugin assets that are always available when the plugin is installed.
All paths use the plugin content prefix /UnrealMCPServer/ instead of /Game/.
"""

# Test Blueprint asset (plugin content)
TEST_BLUEPRINT_PAWN = "/UnrealMCPServer/TestAssets/MCP_TestPawn"

# Test non-Blueprint asset (plugin content)
TEST_MATERIAL = "/UnrealMCPServer/TestAssets/MCP_TestMaterial"

# List of all test assets for validation
ALL_TEST_ASSETS = [
    TEST_BLUEPRINT_PAWN,
    TEST_MATERIAL,
]

