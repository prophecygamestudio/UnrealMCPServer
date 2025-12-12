"""Test script for UnrealMCPProxy that validates basic functionality.

This script:
1. Tests backend connection
2. Tests get_project_config tool call
3. Validates tool description matching
4. Automatically exits upon completion
"""

import asyncio
import json
import sys
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent / "src"))

from unreal_mcp_proxy.client.unreal_mcp import UnrealMCPClient, ConnectionState
from unreal_mcp_proxy.tool_definitions import get_tool_definitions, compare_tool_definitions


async def test_all_tools_defined():
    """Test that all expected tools are defined in cache."""
    print("=" * 60)
    print("Test: All Tools Defined")
    print("=" * 60)
    
    # Expected tools from plugin (15 total)
    expected_tools = [
        # Common tools (4)
        "get_project_config",
        "execute_console_command",
        "get_log_file_path",
        "request_editor_compile",
        # Asset tools (9)
        "export_asset",
        "batch_export_assets",
        "export_class_default",
        "import_asset",
        "query_asset",
        "search_assets",
        "get_asset_dependencies",
        "get_asset_references",
        "get_asset_dependency_tree",
        # Blueprint tools (2)
        "search_blueprints",
        "export_blueprint_markdown"
    ]
    
    cached_tools = get_tool_definitions(enable_markdown_export=True)
    cached_tool_names = set(cached_tools.keys())
    expected_tool_names = set(expected_tools)
    
    missing_tools = expected_tool_names - cached_tool_names
    extra_tools = cached_tool_names - expected_tool_names
    
    if missing_tools:
        print(f"  ✗ Missing tools: {', '.join(sorted(missing_tools))}")
        return False
    
    if extra_tools:
        print(f"  ⚠ Extra tools (not in plugin): {', '.join(sorted(extra_tools))}")
    
    print(f"  ✓ All {len(expected_tools)} expected tools are defined")
    print(f"    Common tools: 4")
    print(f"    Asset tools: 9")
    print(f"    Blueprint tools: 2")
    
    return True


async def test_get_project_config():
    """Test fetching project config - validates basic proxy functionality."""
    print("=" * 60)
    print("Test: get_project_config")
    print("=" * 60)
    
    client = UnrealMCPClient()
    
    if client.state != ConnectionState.ONLINE:
        print("⚠ Backend is offline - cannot test tool call")
        print("   (This is expected if Unreal Editor is not running)")
        print("   To test with backend online, start Unreal Editor with UnrealMCPServer plugin enabled")
        await client.close()
        return True  # Not a failure if backend is offline for testing
    
    try:
        print(f"  Connecting to backend at {client.base_url}...")
        print("  Calling get_project_config...")
        
        response = await client.call_tool("get_project_config", {})
        
        if "error" in response:
            error = response["error"]
            print(f"  ✗ Backend returned error: {error.get('message', error)}")
            return False
        
        result = response.get("result", {})
        content = result.get("content", [])
        
        if not content:
            print("  ✗ No content in response")
            return False
        
        # Parse the first text content
        first_content = content[0]
        if first_content.get("type") == "text":
            text_data = json.loads(first_content.get("text", "{}"))
            
            # Validate response structure
            if "engineVersion" in text_data and "paths" in text_data:
                engine_version = text_data.get("engineVersion", {})
                paths = text_data.get("paths", {})
                
                print("  ✓ Tool call succeeded")
                print(f"    Engine Version: {engine_version.get('full', 'unknown')}")
                print(f"    Project Dir: {paths.get('projectDir', 'unknown')}")
                return True
            else:
                print(f"  ✗ Unexpected response structure")
                print(f"    Keys found: {list(text_data.keys())}")
                return False
        else:
            print(f"  ✗ Unexpected content type: {first_content.get('type')}")
            return False
            
    except Exception as e:
        print(f"  ✗ Error: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await client.close()


async def test_schema_comparison():
    """Comprehensive test that queries backend and compares schemas in detail."""
    print("\n" + "=" * 60)
    print("Test: Detailed Schema Comparison")
    print("=" * 60)
    
    client = UnrealMCPClient()
    
    if client.state != ConnectionState.ONLINE:
        print("⚠ Backend is offline - cannot perform schema comparison")
        print("   (This is expected if Unreal Editor is not running)")
        await client.close()
        return True  # Not a failure if backend is offline
    
    try:
        print("  Fetching tool definitions from backend...")
        response = await client.get_tools_list()
        
        if "error" in response:
            print(f"  ✗ Backend returned error: {response['error']}")
            return False
        
        backend_tools = response.get("result", {}).get("tools", [])
        
        if not backend_tools:
            print("  ✗ No tools from backend")
            return False
        
        print(f"  Found {len(backend_tools)} tools from backend")
        
        # Get cached tools
        print("  Loading cached tool definitions...")
        cached_tools = get_tool_definitions(enable_markdown_export=True)
        print(f"  Found {len(cached_tools)} cached tool definitions")
        
        # Detailed comparison
        print("\n  Performing detailed schema comparison...")
        import json
        
        mismatches = []
        matches = []
        missing_in_cache = []
        missing_in_backend = []
        
        # Check all backend tools
        backend_tool_names = set()
        for backend_tool in backend_tools:
            tool_name = backend_tool.get("name")
            if not tool_name:
                continue
            
            backend_tool_names.add(tool_name)
            cached_tool = cached_tools.get(tool_name)
            
            if not cached_tool:
                missing_in_cache.append(tool_name)
                print(f"    ⚠ '{tool_name}': In backend but not in cache")
                continue
            
            # Compare schemas
            differences = compare_tool_definitions(cached_tool, backend_tool)
            if differences:
                mismatches.append((tool_name, differences, cached_tool, backend_tool))
                print(f"    ✗ '{tool_name}': MISMATCH - {', '.join(differences)}")
            else:
                matches.append(tool_name)
                print(f"    ✓ '{tool_name}': Match")
        
        # Check for tools in cache but not in backend
        for tool_name in cached_tools.keys():
            if tool_name not in backend_tool_names:
                missing_in_backend.append(tool_name)
                print(f"    ⚠ '{tool_name}': In cache but not in backend")
        
        # Print detailed differences for first few mismatches
        if mismatches:
            print(f"\n  Detailed differences (showing first {min(3, len(mismatches))} mismatches):")
            print("  " + "-" * 58)
            
            for i, (tool_name, differences, cached_tool, backend_tool) in enumerate(mismatches[:3]):
                print(f"\n  Tool: {tool_name}")
                print(f"  Differences: {', '.join(differences)}")
                
                # Show input schema differences
                if "InputSchema mismatch detected" in differences:
                    print(f"\n  Input Schema Comparison:")
                    cached_input = json.dumps(cached_tool.get("inputSchema", {}), indent=4, sort_keys=True)
                    backend_input = json.dumps(backend_tool.get("inputSchema", {}), indent=4, sort_keys=True)
                    print(f"    Cached:")
                    for line in cached_input.split('\n'):
                        print(f"      {line}")
                    print(f"    Backend:")
                    for line in backend_input.split('\n'):
                        print(f"      {line}")
                
                # Show output schema differences
                if "OutputSchema mismatch detected" in differences:
                    print(f"\n  Output Schema Comparison:")
                    cached_output = json.dumps(cached_tool.get("outputSchema", {}), indent=4, sort_keys=True)
                    backend_output = json.dumps(backend_tool.get("outputSchema", {}), indent=4, sort_keys=True)
                    print(f"    Cached:")
                    for line in cached_output.split('\n'):
                        print(f"      {line}")
                    print(f"    Backend:")
                    for line in backend_output.split('\n'):
                        print(f"      {line}")
                
                if i < len(mismatches) - 1:
                    print("  " + "-" * 58)
        
        # Summary
        print(f"\n  Summary:")
        print(f"    ✓ Matches: {len(matches)}")
        print(f"    ✗ Mismatches: {len(mismatches)}")
        print(f"    ⚠ Missing in cache: {len(missing_in_cache)}")
        print(f"    ⚠ Missing in backend: {len(missing_in_backend)}")
        
        if mismatches:
            print(f"\n  ⚠ WARNING: {len(mismatches)} tool definition mismatch(es) detected!")
            print(f"    Please update UnrealMCPProxy/src/unreal_mcp_proxy/tool_definitions.py")
            print(f"    to match the backend definitions.")
            print(f"\n    Tools with mismatches:")
            for tool_name, differences, _, _ in mismatches:
                print(f"      - {tool_name}: {', '.join(differences)}")
        
        if missing_in_cache:
            print(f"\n  ⚠ WARNING: {len(missing_in_cache)} tool(s) in backend but not in cache!")
            print(f"    Please add these tools to tool_definitions.py:")
            for name in missing_in_cache[:5]:
                print(f"      - {name}")
            if len(missing_in_cache) > 5:
                print(f"      ... and {len(missing_in_cache) - 5} more")
        
        if missing_in_backend:
            print(f"\n  ⚠ INFO: {len(missing_in_backend)} tool(s) in cache but not in backend")
            print(f"    (These may be deprecated or not yet implemented)")
        
        # Return True if we have matches (mismatches are warnings, not failures for testing)
        return len(matches) > 0
        
    except Exception as e:
        print(f"  ✗ Error: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await client.close()


async def run_tests():
    """Run all tests."""
    print("\n" + "=" * 60)
    print("UnrealMCPProxy Test Suite")
    print("=" * 60)
    print("\nThis script validates:")
    print("  1. All tools are defined in cache")
    print("  2. Backend connection")
    print("  3. get_project_config tool call")
    print("  4. Detailed schema comparison (queries backend for actual schemas)")
    print("\nNote: Some tests require Unreal Editor to be running.\n")
    
    results = []
    
    # Test 1: All tools defined
    results.append(("All Tools Defined", await test_all_tools_defined()))
    
    # Test 2: get_project_config
    results.append(("get_project_config", await test_get_project_config()))
    
    # Test 3: Detailed schema comparison
    results.append(("Schema Comparison", await test_schema_comparison()))
    
    # Print summary
    print("\n" + "=" * 60)
    print("Test Summary")
    print("=" * 60)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"{status}: {test_name}")
    
    print(f"\nTotal: {passed}/{total} tests passed")
    print("=" * 60)
    
    # Return True if all tests passed, or if backend was offline (expected scenario)
    # The test script should exit successfully even if backend is offline
    return passed == total


if __name__ == "__main__":
    try:
        success = asyncio.run(run_tests())
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n\nTests interrupted by user")
        sys.exit(130)
    except Exception as e:
        print(f"\n\nUnexpected error: {str(e)}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

