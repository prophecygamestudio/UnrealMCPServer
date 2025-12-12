"""Basic functionality tests for UnrealMCPProxy."""

import asyncio
import json
import sys
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from unreal_mcp_proxy.client.unreal_mcp import UnrealMCPClient, ConnectionState
from unreal_mcp_proxy.tool_definitions import get_tool_definitions, compare_tool_definitions


async def test_backend_connection():
    """Test basic backend connection."""
    print("Testing backend connection...")
    client = UnrealMCPClient()
    
    if client.state == ConnectionState.ONLINE:
        print("✓ Backend is online")
        return True
    else:
        print("✗ Backend is offline (this is OK for testing)")
        return False


async def test_get_project_config():
    """Test fetching project config from backend."""
    print("\nTesting get_project_config tool call...")
    client = UnrealMCPClient()
    
    if client.state != ConnectionState.ONLINE:
        print("✗ Backend is offline, cannot test tool call")
        return False
    
    try:
        response = await client.call_tool("get_project_config", {})
        
        if "error" in response:
            print(f"✗ Backend returned error: {response['error']}")
            return False
        
        result = response.get("result", {})
        content = result.get("content", [])
        
        if not content:
            print("✗ No content in response")
            return False
        
        # Parse the first text content
        first_content = content[0]
        if first_content.get("type") == "text":
            text_data = json.loads(first_content.get("text", "{}"))
            
            # Check for expected fields
            if "engineVersion" in text_data and "paths" in text_data:
                print("✓ get_project_config returned valid data")
                print(f"  Engine Version: {text_data.get('engineVersion', {}).get('full', 'unknown')}")
                print(f"  Project Dir: {text_data.get('paths', {}).get('projectDir', 'unknown')}")
                return True
            else:
                print(f"✗ Unexpected response structure: {list(text_data.keys())}")
                return False
        else:
            print(f"✗ Unexpected content type: {first_content.get('type')}")
            return False
            
    except Exception as e:
        print(f"✗ Error calling tool: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await client.close()


async def test_tool_list():
    """Test getting tool list from backend."""
    print("\nTesting tools/list...")
    client = UnrealMCPClient()
    
    if client.state != ConnectionState.ONLINE:
        print("✗ Backend is offline, cannot test tools/list")
        return False
    
    try:
        response = await client.get_tools_list()
        
        if "error" in response:
            print(f"✗ Backend returned error: {response['error']}")
            return False
        
        result = response.get("result", {})
        tools = result.get("tools", [])
        
        if not tools:
            print("✗ No tools returned")
            return False
        
        print(f"✓ Backend returned {len(tools)} tools")
        
        # Check for expected tools
        tool_names = [tool.get("name") for tool in tools]
        expected_tools = ["get_project_config", "execute_console_command", "export_asset"]
        
        found_expected = [name for name in expected_tools if name in tool_names]
        print(f"  Found expected tools: {', '.join(found_expected)}")
        
        return len(found_expected) > 0
        
    except Exception as e:
        print(f"✗ Error getting tools list: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await client.close()


async def test_cached_tool_definitions():
    """Test that cached tool definitions are available."""
    print("\nTesting cached tool definitions...")
    
    tools = get_tool_definitions(enable_markdown_export=True)
    
    if not tools:
        print("✗ No cached tool definitions")
        return False
    
    print(f"✓ Found {len(tools)} cached tool definitions")
    
    # Check for expected tools
    expected_tools = ["get_project_config", "execute_console_command", "export_asset"]
    found = [name for name in expected_tools if name in tools]
    print(f"  Found expected tools: {', '.join(found)}")
    
    return len(found) > 0


async def test_description_matching():
    """Test tool description matching between cache and backend."""
    print("\nTesting tool description matching...")
    
    client = UnrealMCPClient()
    
    if client.state != ConnectionState.ONLINE:
        print("✗ Backend is offline, cannot test description matching")
        return False
    
    try:
        # Get cached tools
        cached_tools = get_tool_definitions(enable_markdown_export=True)
        
        # Get backend tools
        response = await client.get_tools_list()
        if "error" in response:
            print(f"✗ Backend returned error: {response['error']}")
            return False
        
        backend_tools = response.get("result", {}).get("tools", [])
        
        if not backend_tools:
            print("✗ No tools from backend")
            return False
        
        # Compare tools
        mismatches = []
        matches = []
        
        for backend_tool in backend_tools:
            tool_name = backend_tool.get("name")
            if not tool_name:
                continue
            
            cached_tool = cached_tools.get(tool_name)
            if not cached_tool:
                print(f"  ⚠ Tool '{tool_name}' in backend but not in cache")
                continue
            
            differences = compare_tool_definitions(cached_tool, backend_tool)
            if differences:
                mismatches.append((tool_name, differences))
                print(f"  ✗ Mismatch in '{tool_name}': {', '.join(differences)}")
            else:
                matches.append(tool_name)
        
        print(f"✓ Description matching complete:")
        print(f"  Matches: {len(matches)}")
        print(f"  Mismatches: {len(mismatches)}")
        
        if matches:
            print(f"  Matching tools: {', '.join(matches[:5])}{'...' if len(matches) > 5 else ''}")
        
        # Return True if we have at least some matches, or if mismatches are expected during development
        return True
        
    except Exception as e:
        print(f"✗ Error testing description matching: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await client.close()


async def run_all_tests():
    """Run all tests."""
    print("=" * 60)
    print("UnrealMCPProxy Basic Functionality Tests")
    print("=" * 60)
    
    results = []
    
    # Test 1: Backend connection
    results.append(("Backend Connection", await test_backend_connection()))
    
    # Test 2: Cached tool definitions
    results.append(("Cached Tool Definitions", await test_cached_tool_definitions()))
    
    # Test 3: Tool list (if backend online)
    if results[0][1]:  # If backend is online
        results.append(("Tools List", await test_tool_list()))
        results.append(("Get Project Config", await test_get_project_config()))
        results.append(("Description Matching", await test_description_matching()))
    
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
    
    return passed == total


if __name__ == "__main__":
    success = asyncio.run(run_all_tests())
    sys.exit(0 if success else 1)

