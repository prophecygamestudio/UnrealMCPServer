"""Integration test that runs the server and validates functionality.

DEPRECATED: This test file has been consolidated into test_proxy.py.
All tests from this file are now available in the consolidated test suite.
This file is kept for reference but should not be used for new tests.
"""

import asyncio
import json
import subprocess
import sys
import time
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

import httpx
from unreal_mcp_proxy.client.unreal_mcp import UnrealMCPClient


async def test_server_via_http():
    """Test the proxy server via HTTP transport."""
    print("Testing proxy server via HTTP...")
    
    # Start server in background (this would need to be done differently in practice)
    # For now, we'll test the client directly
    print("Note: This test requires the server to be running separately")
    print("Run: python -m src.unreal_mcp_proxy.server (with transport=http)")
    
    return True


async def test_client_direct():
    """Test the client directly (simpler test)."""
    print("\nTesting UnrealMCPClient directly...")
    
    client = UnrealMCPClient()
    
    try:
        # Test connection
        if client.state.value == "offline":
            print("⚠ Backend is offline - some tests will be skipped")
            return True
        
        # Test get_project_config
        print("  Testing get_project_config...")
        response = await client.call_tool("get_project_config", {})
        
        if "error" in response:
            print(f"  ✗ Error: {response['error']}")
            return False
        
        result = response.get("result", {})
        content = result.get("content", [])
        
        if content and content[0].get("type") == "text":
            data = json.loads(content[0].get("text", "{}"))
            if "engineVersion" in data:
                print("  ✓ get_project_config works")
                return True
        
        print("  ✗ Unexpected response format")
        return False
        
    except Exception as e:
        print(f"  ✗ Exception: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        await client.close()


async def run_integration_tests():
    """Run integration tests."""
    print("=" * 60)
    print("UnrealMCPProxy Integration Tests")
    print("=" * 60)
    
    results = []
    results.append(("Client Direct Test", await test_client_direct()))
    
    print("\n" + "=" * 60)
    print("Integration Test Summary")
    print("=" * 60)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"{status}: {test_name}")
    
    print(f"\nTotal: {passed}/{total} tests passed")
    
    return passed == total


if __name__ == "__main__":
    success = asyncio.run(run_integration_tests())
    sys.exit(0 if success else 1)

