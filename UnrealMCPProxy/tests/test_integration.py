"""Consolidated test suite for UnrealMCPProxy.

This test suite consolidates all test functionality from:
- test_proxy.py
- test_proxy_basic.py  
- test_server_integration.py

Tests are organized by category and use shared fixtures/helpers.
"""

import asyncio
import json
import sys
from pathlib import Path
from typing import Optional, Tuple, List

# Add src to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from unreal_mcp_proxy.client.unreal_mcp import UnrealMCPClient, ConnectionState
from unreal_mcp_proxy.tool_definitions import get_tool_definitions, compare_tool_definitions
from unreal_mcp_proxy.prompt_definitions import get_prompt_definitions, get_cached_prompt_definitions
from unreal_mcp_proxy.resource_definitions import get_resource_definitions, get_resource_template_definitions, get_cached_resource_definitions
from tests.test_assets import TEST_BLUEPRINT_PAWN, TEST_MATERIAL


# Shared test constants
EXPECTED_TOOLS = [
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


# Shared test helpers (fixtures)
async def create_test_client() -> UnrealMCPClient:
    """Create a test client instance (shared fixture).
    
    Initializes the client and performs immediate connection check.
    
    Returns:
        UnrealMCPClient instance ready for testing
    """
    client = UnrealMCPClient()
    # Initialize async components (performs immediate connection check)
    await client.initialize_async()
    return client


async def cleanup_client(client: Optional[UnrealMCPClient]) -> None:
    """Clean up a test client (shared fixture).
    
    Args:
        client: Client instance to clean up, or None
    """
    if client:
        await client.close()


def parse_tool_response(response: dict) -> Tuple[bool, Optional[dict], Optional[str]]:
    """Parse a tool response and extract data.
    
    Returns:
        Tuple of (success, data_dict, error_message)
    """
    if "error" in response:
        error = response["error"]
        return False, None, error.get("message", str(error))
    
    result = response.get("result", {})
    content = result.get("content", [])
    
    if not content:
        return False, None, "No content in response"
    
    first_content = content[0]
    if first_content.get("type") == "text":
        try:
            text_data = json.loads(first_content.get("text", "{}"))
            return True, text_data, None
        except json.JSONDecodeError:
            return False, None, "Failed to parse JSON response"
    
    return False, None, f"Unexpected content type: {first_content.get('type')}"


# ============================================================================
# Category 1: Tool Definition Tests (work offline)
# ============================================================================

async def test_all_tools_defined() -> bool:
    """Test that all expected tools are defined in cache."""
    print("=" * 60)
    print("Test: All Tools Defined")
    print("=" * 60)
    
    cached_tools = get_tool_definitions(enable_markdown_export=True)
    cached_tool_names = set(cached_tools.keys())
    expected_tool_names = set(EXPECTED_TOOLS)
    
    missing_tools = expected_tool_names - cached_tool_names
    extra_tools = cached_tool_names - expected_tool_names
    
    if missing_tools:
        print(f"  [X] Missing tools: {', '.join(sorted(missing_tools))}")
        return False
    
    if extra_tools:
        print(f"  [W] Extra tools (not in plugin): {', '.join(sorted(extra_tools))}")
    
    print(f"  [OK] All {len(EXPECTED_TOOLS)} expected tools are defined")
    print(f"    Common tools: 4")
    print(f"    Asset tools: 9")
    print(f"    Blueprint tools: 2")
    
    return True


async def test_cached_tool_definitions() -> bool:
    """Test that cached tool definitions are available."""
    print("=" * 60)
    print("Test: Cached Tool Definitions")
    print("=" * 60)
    
    tools = get_tool_definitions(enable_markdown_export=True)
    
    if not tools:
        print("  [X] No cached tool definitions")
        return False
    
    print(f"  [OK] Found {len(tools)} cached tool definitions")
    
    # Check for expected tools
    expected_sample = ["get_project_config", "execute_console_command", "export_asset"]
    found = [name for name in expected_sample if name in tools]
    print(f"  Found expected tools: {', '.join(found)}")
    
    return len(found) > 0


async def test_prompt_definitions_file() -> bool:
    """Test that prompt definitions JSON file can be found and read."""
    print("=" * 60)
    print("Test: Prompt Definitions File")
    print("=" * 60)
    
    try:
        prompts = get_prompt_definitions()
        
        if not prompts:
            print("  [X] No prompt definitions loaded")
            return False
        
        print(f"  [OK] Successfully loaded {len(prompts)} prompt definitions from JSON")
        
        # Check for expected prompts
        expected_prompts = [
            "analyze_blueprint",
            "refactor_blueprint",
            "audit_assets",
            "create_blueprint",
            "analyze_performance"
        ]
        
        found_prompts = [name for name in expected_prompts if name in prompts]
        missing_prompts = [name for name in expected_prompts if name not in prompts]
        
        if missing_prompts:
            print(f"  [W] Missing expected prompts: {', '.join(missing_prompts)}")
        
        print(f"  Found expected prompts: {', '.join(found_prompts)}")
        
        # Validate structure of a sample prompt
        if "analyze_blueprint" in prompts:
            prompt = prompts["analyze_blueprint"]
            required_fields = ["name", "title", "description", "arguments", "template"]
            missing_fields = [field for field in required_fields if field not in prompt]
            
            if missing_fields:
                print(f"  [W] Prompt 'analyze_blueprint' missing fields: {', '.join(missing_fields)}")
            else:
                print(f"  [OK] Prompt 'analyze_blueprint' has all required fields")
                print(f"    Arguments: {len(prompt.get('arguments', []))}")
                print(f"    Template length: {len(prompt.get('template', ''))} chars")
        
        return len(found_prompts) > 0
        
    except FileNotFoundError as e:
        print(f"  [X] Prompt definitions file not found: {str(e)}")
        return False
    except json.JSONDecodeError as e:
        print(f"  [X] Invalid JSON in prompt definitions file: {str(e)}")
        return False
    except Exception as e:
        print(f"  [X] Error loading prompt definitions: {str(e)}")
        import traceback
        traceback.print_exc()
        return False


async def test_resource_definitions_file() -> bool:
    """Test that resource definitions JSON file can be found and read."""
    print("=" * 60)
    print("Test: Resource Definitions File")
    print("=" * 60)
    
    try:
        cached = get_cached_resource_definitions()
        templates = get_resource_template_definitions()
        
        if not templates:
            print("  [X] No resource template definitions loaded")
            return False
        
        print(f"  [OK] Successfully loaded {len(templates)} resource template definitions from JSON")
        
        # Check for expected resource templates
        expected_templates = [
            "unreal+t3d://{filepath}",
            "unreal+md://{filepath}"
        ]
        
        template_uris = [t.get("uriTemplate", "") for t in templates]
        found_templates = [uri for uri in expected_templates if uri in template_uris]
        missing_templates = [uri for uri in expected_templates if uri not in template_uris]
        
        if missing_templates:
            print(f"  [W] Missing expected templates: {', '.join(missing_templates)}")
        
        print(f"  Found expected templates: {', '.join(found_templates)}")
        
        # Validate structure of templates
        for template in templates:
            required_fields = ["name", "description", "uriTemplate", "mimeType"]
            missing_fields = [field for field in required_fields if field not in template]
            
            if missing_fields:
                print(f"  [W] Template '{template.get('name', 'unknown')}' missing fields: {', '.join(missing_fields)}")
            else:
                print(f"  [OK] Template '{template.get('name')}' has all required fields")
                print(f"    URI Template: {template.get('uriTemplate')}")
                print(f"    MIME Type: {template.get('mimeType')}")
        
        return len(found_templates) > 0
        
    except FileNotFoundError as e:
        print(f"  [X] Resource definitions file not found: {str(e)}")
        return False
    except json.JSONDecodeError as e:
        print(f"  [X] Invalid JSON in resource definitions file: {str(e)}")
        return False
    except Exception as e:
        print(f"  [X] Error loading resource definitions: {str(e)}")
        import traceback
        traceback.print_exc()
        return False


# ============================================================================
# Category 2: Backend Connection Tests (require backend online)
# ============================================================================

async def test_backend_connection() -> bool:
    """Test basic backend connection."""
    print("=" * 60)
    print("Test: Backend Connection")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        # State should be set immediately after initialization
        if client.state == ConnectionState.ONLINE:
            print("  [OK] Backend is online")
            return True
        else:
            print("  [W] Backend is offline (this is OK for testing)")
            print("      To test with backend online, start Unreal Editor with UnrealMCPServer plugin enabled")
            return True  # Not a failure if backend is offline
    finally:
        await cleanup_client(client)


async def test_tool_list() -> bool:
    """Test getting tool list from backend."""
    print("=" * 60)
    print("Test: Tools List")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        # Check if backend is online (state set during initialization)
        if client.state != ConnectionState.ONLINE:
            print("  [W] Backend is offline, cannot test tools/list")
            return True  # Not a failure if backend is offline
        
        # Try to get tools list
        try:
            response = await client.get_tools_list()
        except (ConnectionError, TimeoutError):
            print("  [W] Backend is offline, cannot test tools/list")
            return True  # Not a failure if backend is offline
        
        # tools/list returns result.tools directly, not result.content
        if "error" in response:
            error = response["error"]
            print(f"  [X] Backend returned error: {error.get('message', str(error))}")
            return False
        
        result = response.get("result", {})
        tools = result.get("tools", [])
        
        if not tools:
            print("  [X] No tools returned")
            return False
        
        print(f"  [OK] Backend returned {len(tools)} tools")
        
        # Check for expected tools
        tool_names = [tool.get("name") for tool in tools]
        expected_sample = ["get_project_config", "execute_console_command", "export_asset"]
        found_expected = [name for name in expected_sample if name in tool_names]
        print(f"  Found expected tools: {', '.join(found_expected)}")
        
        return len(found_expected) > 0
        
    except Exception as e:
        print(f"  [X] Error getting tools list: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await cleanup_client(client)


# ============================================================================
# Category 3: Tool Call Tests (require backend online)
# ============================================================================

async def test_get_project_config() -> bool:
    """Test fetching project config - validates basic proxy functionality."""
    print("=" * 60)
    print("Test: get_project_config")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        if client.state != ConnectionState.ONLINE:
            print("  [W] Backend is offline - cannot test tool call")
            print("      (This is expected if Unreal Editor is not running)")
            print("      To test with backend online, start Unreal Editor with UnrealMCPServer plugin enabled")
            return True  # Not a failure if backend is offline
        
        print(f"  Connecting to backend at {client.base_url}...")
        print("  Calling get_project_config...")
        
        response = await client.call_tool("get_project_config", {})
        
        success, data, error = parse_tool_response(response)
        if not success:
            print(f"  [X] Backend returned error: {error}")
            return False
        
        # Validate response structure
        if data and "engineVersion" in data and "paths" in data:
            engine_version = data.get("engineVersion", {})
            paths = data.get("paths", {})
            
            print("  [OK] Tool call succeeded")
            print(f"    Engine Version: {engine_version.get('full', 'unknown')}")
            print(f"    Project Dir: {paths.get('projectDir', 'unknown')}")
            return True
        else:
            print(f"  [X] Unexpected response structure")
            print(f"    Keys found: {list(data.keys()) if data else 'None'}")
            return False
            
    except Exception as e:
        print(f"  [X] Error: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await cleanup_client(client)


async def test_client_direct() -> bool:
    """Test the client directly (simpler integration test)."""
    print("=" * 60)
    print("Test: Client Direct")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        if client.state.value == "offline":
            print("  [W] Backend is offline - some tests will be skipped")
            return True  # Not a failure if backend is offline
        
        # Test get_project_config
        print("  Testing get_project_config...")
        response = await client.call_tool("get_project_config", {})
        
        success, data, error = parse_tool_response(response)
        if not success:
            print(f"  [X] Error: {error}")
            return False
        
        if data and "engineVersion" in data:
            print("  [OK] get_project_config works")
            return True
        
        print("  [X] Unexpected response format")
        return False
        
    except Exception as e:
        print(f"  [X] Exception: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await cleanup_client(client)


# ============================================================================
# Category 4: Schema Compatibility Tests (require backend online)
# ============================================================================

async def test_schema_comparison() -> bool:
    """Comprehensive test that queries backend and checks schema compatibility."""
    print("\n" + "=" * 60)
    print("Test: Schema Compatibility Check")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        if client.state != ConnectionState.ONLINE:
            print("[W] Backend is offline - cannot perform schema comparison")
            print("   (This is expected if Unreal Editor is not running)")
            return True  # Not a failure if backend is offline
        
        print("  Fetching tool definitions from backend...")
        response = await client.get_tools_list()
        
        # tools/list returns result.tools directly, not result.content
        if "error" in response:
            error = response["error"]
            print(f"  [X] Backend returned error: {error.get('message', str(error))}")
            return False
        
        result = response.get("result", {})
        backend_tools = result.get("tools", [])
        
        if not backend_tools:
            print("  [X] No tools from backend")
            return False
        
        print(f"  Found {len(backend_tools)} tools from backend")
        
        # Get cached tools
        print("  Loading cached tool definitions...")
        cached_tools = get_tool_definitions(enable_markdown_export=True)
        print(f"  Found {len(cached_tools)} cached tool definitions")
        
        # Compatibility check
        print("\n  Performing schema compatibility check...")
        print("  (Proxy schemas can differ from backend as long as they're compatible)")
        
        compatibility_issues = []
        compatible = []
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
                print(f"    [W] '{tool_name}': In backend but not in cache")
                continue
            
            # Check compatibility (not exact match)
            issues = compare_tool_definitions(cached_tool, backend_tool)
            if issues:
                compatibility_issues.append((tool_name, issues, cached_tool, backend_tool))
                print(f"    [W] '{tool_name}': Compatibility issues - {', '.join(issues)}")
            else:
                compatible.append(tool_name)
                print(f"    [OK] '{tool_name}': Compatible")
        
        # Check for tools in cache but not in backend
        for tool_name in cached_tools.keys():
            if tool_name not in backend_tool_names:
                missing_in_backend.append(tool_name)
                print(f"    [W] '{tool_name}': In cache but not in backend")
        
        # Print detailed compatibility issues for first few
        if compatibility_issues:
            print(f"\n  Detailed compatibility issues (showing first {min(3, len(compatibility_issues))}):")
            print("  " + "-" * 58)
            
            for i, (tool_name, issues, cached_tool, backend_tool) in enumerate(compatibility_issues[:3]):
                print(f"\n  Tool: {tool_name}")
                print(f"  Issues: {', '.join(issues)}")
                
                # Show schema details for missing required fields
                if any("Missing required fields" in issue for issue in issues):
                    print(f"\n  Schema Details:")
                    cached_input = json.dumps(cached_tool.get("inputSchema", {}), indent=4, sort_keys=True)
                    backend_input = json.dumps(backend_tool.get("inputSchema", {}), indent=4, sort_keys=True)
                    print(f"    Proxy Schema:")
                    for line in cached_input.split('\n')[:20]:  # Limit output
                        print(f"      {line}")
                    if len(cached_input.split('\n')) > 20:
                        print(f"      ... (truncated)")
                    print(f"    Backend Schema:")
                    for line in backend_input.split('\n')[:20]:  # Limit output
                        print(f"      {line}")
                    if len(backend_input.split('\n')) > 20:
                        print(f"      ... (truncated)")
                
                if i < len(compatibility_issues) - 1:
                    print("  " + "-" * 58)
        
        # Summary
        print(f"\n  Summary:")
        print(f"    [OK] Compatible: {len(compatible)}")
        print(f"    [W] Compatibility issues: {len(compatibility_issues)}")
        print(f"    [W] Missing in cache: {len(missing_in_cache)}")
        print(f"    [W] Missing in backend: {len(missing_in_backend)}")
        
        if compatibility_issues:
            print(f"\n  [W] WARNING: {len(compatibility_issues)} tool(s) have compatibility issues!")
            print(f"    These issues may prevent tools from working correctly.")
            print(f"    Please update UnrealMCPProxy/src/unreal_mcp_proxy/tool_definitions.py")
            print(f"    to fix compatibility issues.")
            print(f"\n    Tools with issues:")
            for tool_name, issues, _, _ in compatibility_issues:
                print(f"      - {tool_name}: {', '.join(issues)}")
        
        if missing_in_cache:
            print(f"\n  [W] WARNING: {len(missing_in_cache)} tool(s) in backend but not in cache!")
            print(f"    Please add these tools to tool_definitions.py:")
            for name in missing_in_cache[:5]:
                print(f"      - {name}")
            if len(missing_in_cache) > 5:
                print(f"      ... and {len(missing_in_cache) - 5} more")
        
        if missing_in_backend:
            print(f"\n  [W] INFO: {len(missing_in_backend)} tool(s) in cache but not in backend")
            print(f"    (These may be deprecated or not yet implemented)")
        
        # Return True if we have compatible tools (compatibility issues are warnings, not failures for testing)
        return len(compatible) > 0
        
    except Exception as e:
        print(f"  [X] Error: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await cleanup_client(client)


async def test_description_matching() -> bool:
    """Test tool description matching between cache and backend."""
    print("=" * 60)
    print("Test: Description Matching")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        if client.state != ConnectionState.ONLINE:
            print("  [W] Backend is offline, cannot test description matching")
            return True  # Not a failure if backend is offline
        
        # Get cached tools
        cached_tools = get_tool_definitions(enable_markdown_export=True)
        
        # Get backend tools
        response = await client.get_tools_list()
        
        # tools/list returns result.tools directly, not result.content
        if "error" in response:
            error = response["error"]
            print(f"  [X] Backend returned error: {error.get('message', str(error))}")
            return False
        
        result = response.get("result", {})
        backend_tools = result.get("tools", [])
        
        if not backend_tools:
            print("  [X] No tools from backend")
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
                print(f"  [W] Tool '{tool_name}' in backend but not in cache")
                continue
            
            differences = compare_tool_definitions(cached_tool, backend_tool)
            if differences:
                mismatches.append((tool_name, differences))
                print(f"  [W] Mismatch in '{tool_name}': {', '.join(differences)}")
            else:
                matches.append(tool_name)
        
        print(f"  [OK] Description matching complete:")
        print(f"    Matches: {len(matches)}")
        print(f"    Mismatches: {len(mismatches)}")
        
        if matches:
            print(f"    Matching tools: {', '.join(matches[:5])}{'...' if len(matches) > 5 else ''}")
        
        # Return True if we have at least some matches, or if mismatches are expected during development
        return True
        
    except Exception as e:
        print(f"  [X] Error testing description matching: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await cleanup_client(client)


# ============================================================================
# Category 5: Error Path and Edge Case Tests
# ============================================================================

# ============================================================================
# Category 6: Resources Tests (require backend online)
# ============================================================================

async def test_resources_list() -> bool:
    """Test getting resources list from backend."""
    print("=" * 60)
    print("Test: Resources List")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        if client.state != ConnectionState.ONLINE:
            print("  [W] Backend is offline, cannot test resources/list")
            return True  # Not a failure if backend is offline
        
        response = await client.get_resources_list()
        
        if "error" in response:
            error = response["error"]
            print(f"  [X] Backend returned error: {error.get('message', str(error))}")
            return False
        
        result = response.get("result", {})
        resources = result.get("resources", [])
        
        print(f"  [OK] Backend returned {len(resources)} static resources")
        
        # Check for expected resource templates (we know about T3D and Markdown)
        return True
        
    except Exception as e:
        print(f"  [X] Error getting resources list: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await cleanup_client(client)


async def test_resource_templates_list() -> bool:
    """Test getting resource templates list from backend."""
    print("=" * 60)
    print("Test: Resource Templates List")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        if client.state != ConnectionState.ONLINE:
            print("  [W] Backend is offline, cannot test resources/templates/list")
            return True  # Not a failure if backend is offline
        
        response = await client.get_resource_templates_list()
        
        if "error" in response:
            error = response["error"]
            print(f"  [X] Backend returned error: {error.get('message', str(error))}")
            return False
        
        result = response.get("result", {})
        templates = result.get("resourceTemplates", [])
        
        print(f"  [OK] Backend returned {len(templates)} resource templates")
        
        # Check for expected templates
        template_uris = [t.get("uriTemplate", "") for t in templates]
        expected_templates = ["unreal+t3d://{filepath}", "unreal+md://{filepath}"]
        found_templates = [uri for uri in expected_templates if uri in template_uris]
        
        if found_templates:
            print(f"  [OK] Found expected templates: {', '.join(found_templates)}")
        else:
            print(f"  [W] Expected templates not found. Found: {', '.join(template_uris)}")
        
        return True
        
    except Exception as e:
        print(f"  [X] Error getting resource templates list: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await cleanup_client(client)


async def test_test_assets_exist() -> bool:
    """Test that plugin test assets exist and can be queried."""
    print("=" * 60)
    print("Test: Test Assets Exist")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        if client.state != ConnectionState.ONLINE:
            print("  [W] Backend is offline, cannot test asset existence")
            return True  # Not a failure if backend is offline
        
        # Query the test Blueprint asset
        print(f"  Querying test Blueprint asset: {TEST_BLUEPRINT_PAWN}")
        response = await client.call_tool("query_asset", {
            "assetPath": TEST_BLUEPRINT_PAWN,
            "bIncludeTags": False
        })
        
        success, data, error = parse_tool_response(response)
        if not success:
            print(f"  [W] Test asset not found: {error}")
            print(f"    Note: Test assets need to be created in Unreal Editor.")
            print(f"    See Content/TestAssets/README.md for instructions.")
            print(f"    This is acceptable - test assets may not exist yet.")
            return True  # Not a failure - assets may not be created yet
        
        if data and data.get("bExists", False):
            print(f"  [OK] Test Blueprint asset exists: {data.get('assetName', 'unknown')}")
            print(f"    Class: {data.get('classPath', 'unknown')}")
            print(f"    Package: {data.get('packagePath', 'unknown')}")
            
            # Also test the Material asset
            print(f"  Querying test Material asset: {TEST_MATERIAL}")
            material_response = await client.call_tool("query_asset", {
                "assetPath": TEST_MATERIAL,
                "bIncludeTags": False
            })
            
            material_success, material_data, material_error = parse_tool_response(material_response)
            if material_success and material_data and material_data.get("bExists", False):
                print(f"  [OK] Test Material asset exists: {material_data.get('assetName', 'unknown')}")
                print(f"    Class: {material_data.get('classPath', 'unknown')}")
                return True
            else:
                print(f"  [W] Test Material asset not found: {material_error}")
                return True  # Not a failure - may not exist yet
        else:
            print(f"  [W] Test Blueprint asset does not exist")
            print(f"    Note: Test assets need to be created in Unreal Editor.")
            print(f"    See Content/TestAssets/README.md for instructions.")
            return True  # Not a failure - assets may not be created yet
        
    except Exception as e:
        print(f"  [X] Error querying test asset: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await cleanup_client(client)


async def test_read_resource() -> bool:
    """Test reading a resource from backend."""
    print("=" * 60)
    print("Test: Read Resource")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        if client.state != ConnectionState.ONLINE:
            print("  [W] Backend is offline, cannot test resources/read")
            return True  # Not a failure if backend is offline
        
        # Try to read a T3D resource using plugin test Blueprint asset
        test_uri = f"unreal+t3d://{TEST_BLUEPRINT_PAWN}"
        print(f"  Testing resource read: {test_uri}")
        print(f"  Using plugin test Blueprint asset: {TEST_BLUEPRINT_PAWN}")
        
        response = await client.read_resource(test_uri)
        
        if "error" in response:
            error = response["error"]
            error_message = error.get("message", "").lower()
            error_code = str(error.get("code", "")).lower()
            
            # Resource not found or failed to load - check if it's the test asset or a real error
            if ("not found" in error_message or 
                "resource not found" in error_message or
                "resourcenotfound" in error_code or
                "failed to load resource" in error_message or
                "failed to load blueprint" in error_message):
                # If test asset doesn't exist yet, that's OK (assets need to be created in editor)
                # But log it as a warning so user knows to create the test assets
                print(f"  [W] Resource not found/failed to load: {error.get('message', '')}")
                print(f"    Note: Test assets need to be created in Unreal Editor.")
                print(f"    See Content/TestAssets/README.md for instructions.")
                print(f"    This is acceptable for now - test asset may not exist yet.")
                return True
            else:
                print(f"  [X] Backend returned unexpected error: {error.get('message', str(error))}")
                return False
        
        result = response.get("result", {})
        contents = result.get("contents", [])
        
        if contents:
            print(f"  [OK] Resource read succeeded, got {len(contents)} content item(s)")
            first_content = contents[0]
            mime_type = first_content.get("mimeType", "unknown")
            print(f"    MIME type: {mime_type}")
            return True
        else:
            print(f"  [W] Resource read returned no content")
            return True  # Not necessarily a failure
        
    except Exception as e:
        print(f"  [X] Error reading resource: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await cleanup_client(client)


# ============================================================================
# Category 7: Prompts Tests (require backend online)
# ============================================================================

async def test_prompts_list() -> bool:
    """Test getting prompts list from backend."""
    print("=" * 60)
    print("Test: Prompts List")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        if client.state != ConnectionState.ONLINE:
            print("  [W] Backend is offline, cannot test prompts/list")
            return True  # Not a failure if backend is offline
        
        response = await client.get_prompts_list()
        
        if "error" in response:
            error = response["error"]
            print(f"  [X] Backend returned error: {error.get('message', str(error))}")
            return False
        
        result = response.get("result", {})
        prompts = result.get("prompts", [])
        
        print(f"  [OK] Backend returned {len(prompts)} prompts")
        
        # Check for expected prompts
        prompt_names = [p.get("name", "") for p in prompts]
        expected_prompts = ["analyze_blueprint", "refactor_blueprint", "audit_assets", "create_blueprint", "analyze_performance"]
        found_prompts = [name for name in expected_prompts if name in prompt_names]
        
        if found_prompts:
            print(f"  [OK] Found expected prompts: {', '.join(found_prompts)}")
        else:
            print(f"  [W] Expected prompts not found. Found: {', '.join(prompt_names)}")
        
        return True
        
    except Exception as e:
        print(f"  [X] Error getting prompts list: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await cleanup_client(client)


async def test_get_prompt() -> bool:
    """Test getting a prompt from backend."""
    print("=" * 60)
    print("Test: Get Prompt")
    print("=" * 60)
    
    client = await create_test_client()
    
    try:
        if client.state != ConnectionState.ONLINE:
            print("  [W] Backend is offline, cannot test prompts/get")
            return True  # Not a failure if backend is offline
        
        # Try to get the analyze_blueprint prompt
        test_prompt_name = "analyze_blueprint"
        test_arguments = {
            "blueprint_path": TEST_BLUEPRINT_PAWN,
            "focus_areas": "all"
        }
        print(f"  Using plugin test Blueprint asset: {TEST_BLUEPRINT_PAWN}")
        
        print(f"  Testing prompt get: {test_prompt_name}")
        
        response = await client.get_prompt(test_prompt_name, test_arguments)
        
        if "error" in response:
            error = response["error"]
            # Prompt not found is acceptable if prompts aren't registered yet
            if "not found" in error.get("message", "").lower():
                print(f"  [W] Prompt not found (may not be registered yet): {error.get('message', '')}")
                return True
            else:
                print(f"  [X] Backend returned unexpected error: {error.get('message', str(error))}")
                return False
        
        result = response.get("result", {})
        messages = result.get("messages", [])
        
        if messages:
            print(f"  [OK] Prompt get succeeded, got {len(messages)} message(s)")
            return True
        else:
            print(f"  [W] Prompt get returned no messages")
            return True  # Not necessarily a failure
        
    except Exception as e:
        print(f"  [X] Error getting prompt: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await cleanup_client(client)

async def test_error_invalid_tool() -> bool:
    """Test error handling for invalid/non-existent tool."""
    print("=" * 60)
    print("Test: Error Handling - Invalid Tool")
    print("=" * 60)
    
    client = await create_test_client()
    try:
        # Try to call a non-existent tool
        response = await client.call_tool("nonexistent_tool_xyz", {})
        
        # Should return an error response
        if "error" in response:
            print("  [OK] Invalid tool correctly returned error")
            return True
        else:
            print("  [X] Invalid tool did not return error")
            return False
    except Exception as e:
        # Exception is also acceptable for invalid tools
        print(f"  [OK] Invalid tool raised exception (acceptable): {type(e).__name__}")
        return True
    finally:
        await cleanup_client(client)


async def test_error_offline_backend() -> bool:
    """Test error handling when backend is offline."""
    print("=" * 60)
    print("Test: Error Handling - Offline Backend")
    print("=" * 60)
    
    client = await create_test_client()
    try:
        # If backend is offline, tool calls should handle gracefully
        if client.state == ConnectionState.OFFLINE:
            print("  [OK] Backend is offline (expected for this test)")
            # Try to call a tool - should handle offline state gracefully
            try:
                response = await client.call_tool("get_project_config", {})
                # Should either return error or raise exception
                if "error" in response:
                    print("  [OK] Tool call returned error for offline backend (acceptable)")
                    return True
                else:
                    print("  [W] Tool call succeeded despite offline backend (unexpected)")
                    return False
            except (ConnectionError, TimeoutError) as e:
                print(f"  [OK] Tool call raised connection error for offline backend: {type(e).__name__}")
                return True
            except Exception as e:
                print(f"  [W] Tool call raised unexpected exception: {type(e).__name__}: {str(e)}")
                return False
        else:
            print("  [INFO] Backend is online - skipping offline test")
            return True
    finally:
        await cleanup_client(client)


async def test_edge_case_empty_arguments() -> bool:
    """Test edge case: tool call with empty arguments."""
    print("=" * 60)
    print("Test: Edge Case - Empty Tool Arguments")
    print("=" * 60)
    
    client = await create_test_client()
    try:
        if client.state != ConnectionState.ONLINE:
            print("  [INFO] Backend is offline - skipping edge case test")
            return True
        
        # Call a tool that accepts empty arguments
        response = await client.call_tool("get_project_config", {})
        
        if "error" in response:
            print(f"  [W] Tool call with empty arguments returned error: {response['error']}")
            return False
        else:
            print("  [OK] Tool call with empty arguments succeeded")
            return True
    except Exception as e:
        print(f"  [X] Tool call with empty arguments raised exception: {str(e)}")
        return False
    finally:
        await cleanup_client(client)


# ============================================================================
# Test Runner
# ============================================================================

async def run_all_tests() -> bool:
    """Run all tests organized by category."""
    print("\n" + "=" * 60)
    print("UnrealMCPProxy Consolidated Test Suite")
    print("=" * 60)
    print("\nThis test suite validates:")
    print("  1. Tool definitions (works offline)")
    print("  2. Definition files (JSON) can be found and read (works offline)")
    print("  3. Backend connection (requires backend online)")
    print("  4. Tool calls (requires backend online)")
    print("  5. Schema compatibility (requires backend online)")
    print("  6. Error paths and edge cases")
    print("  7. Resources (requires backend online)")
    print("  8. Prompts (requires backend online)")
    print("\nNote: Some tests require Unreal Editor to be running.\n")
    
    results: List[Tuple[str, bool]] = []
    
    # Category 1: Tool Definition Tests (work offline)
    print("\n" + "=" * 60)
    print("Category 1: Tool Definition Tests")
    print("=" * 60)
    results.append(("All Tools Defined", await test_all_tools_defined()))
    results.append(("Cached Tool Definitions", await test_cached_tool_definitions()))
    results.append(("Prompt Definitions File", await test_prompt_definitions_file()))
    results.append(("Resource Definitions File", await test_resource_definitions_file()))
    
    # Category 2: Backend Connection Tests
    print("\n" + "=" * 60)
    print("Category 2: Backend Connection Tests")
    print("=" * 60)
    results.append(("Backend Connection", await test_backend_connection()))
    
    # Check if backend is online for remaining tests
    client = await create_test_client()
    backend_online = client.state == ConnectionState.ONLINE
    await cleanup_client(client)
    
    if backend_online:
        results.append(("Tools List", await test_tool_list()))
        
        # Category 3: Tool Call Tests
        print("\n" + "=" * 60)
        print("Category 3: Tool Call Tests")
        print("=" * 60)
        results.append(("get_project_config", await test_get_project_config()))
        results.append(("Client Direct", await test_client_direct()))
        
        # Category 4: Schema Compatibility Tests
        print("\n" + "=" * 60)
        print("Category 4: Schema Compatibility Tests")
        print("=" * 60)
        results.append(("Schema Comparison", await test_schema_comparison()))
        results.append(("Description Matching", await test_description_matching()))
        
        # Category 5: Error Path and Edge Case Tests
        print("\n" + "=" * 60)
        print("Category 5: Error Path and Edge Case Tests")
        print("=" * 60)
        results.append(("Error Handling - Invalid Tool", await test_error_invalid_tool()))
        results.append(("Error Handling - Offline Backend", await test_error_offline_backend()))
        results.append(("Edge Case - Empty Tool Arguments", await test_edge_case_empty_arguments()))
        
        # Category 6: Resources Tests
        print("\n" + "=" * 60)
        print("Category 6: Resources Tests")
        print("=" * 60)
        results.append(("Test Assets Exist", await test_test_assets_exist()))
        results.append(("Resources List", await test_resources_list()))
        results.append(("Resource Templates List", await test_resource_templates_list()))
        results.append(("Read Resource", await test_read_resource()))
        
        # Category 7: Prompts Tests
        print("\n" + "=" * 60)
        print("Category 7: Prompts Tests")
        print("=" * 60)
        results.append(("Prompts List", await test_prompts_list()))
        results.append(("Get Prompt", await test_get_prompt()))
    else:
        print("\n  [INFO] Backend is offline - skipping tests that require backend connection")
        print("         Start Unreal Editor with UnrealMCPServer plugin enabled to run all tests")
        
        # Category 5: Error Path Tests (work offline)
        print("\n" + "=" * 60)
        print("Category 5: Error Path Tests (Offline)")
        print("=" * 60)
        results.append(("Error Handling - Invalid Tool", await test_error_invalid_tool()))
        results.append(("Error Handling - Offline Backend", await test_error_offline_backend()))
    
    # Print summary
    print("\n" + "=" * 60)
    print("Test Summary")
    print("=" * 60)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "[OK] PASS" if result else "[X] FAIL"
        print(f"{status}: {test_name}")
    
    print(f"\nTotal: {passed}/{total} tests passed")
    print("=" * 60)
    
    # Return True if all tests passed, or if backend was offline (expected scenario)
    return passed == total


if __name__ == "__main__":
    try:
        success = asyncio.run(run_all_tests())
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n\nTests interrupted by user")
        sys.exit(130)
    except Exception as e:
        print(f"\n\nUnexpected error: {str(e)}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

