"""Unit tests with mocks for UnrealMCPProxy core functions.

These tests use mocks to test functions in isolation without requiring
a running backend or full integration setup.
"""

import sys
from pathlib import Path
from unittest.mock import Mock, patch, AsyncMock, MagicMock
import pytest
import json

# Add src to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from unreal_mcp_proxy.tool_definitions import compare_tool_definitions
from unreal_mcp_proxy.tools import (
    is_read_only_tool,
    is_transient_error,
    handle_tool_result,
    call_tool_with_retry
)
from unreal_mcp_proxy.errors import create_error_response
from unreal_mcp_proxy.tool_decorators import read_only, write_operation, get_tool_read_only_flag


# ============================================================================
# Test compare_tool_definitions
# ============================================================================

def test_compare_tool_definitions_compatible():
    """Test that compatible tool definitions return no issues."""
    proxy_tool = {
        "name": "test_tool",
        "inputSchema": {
            "type": "object",
            "properties": {
                "param1": {"type": "string"},
                "param2": {"type": "integer"}
            },
            "required": ["param1"]
        }
    }
    
    backend_tool = {
        "name": "test_tool",
        "inputSchema": {
            "type": "object",
            "properties": {
                "param1": {"type": "string"},
                "param2": {"type": "number"}  # number and integer are compatible
            },
            "required": ["param1"]
        }
    }
    
    issues = compare_tool_definitions(proxy_tool, backend_tool)
    assert len(issues) == 0, f"Expected no issues, got: {issues}"


def test_compare_tool_definitions_missing_required():
    """Test that missing required fields are detected."""
    proxy_tool = {
        "name": "test_tool",
        "inputSchema": {
            "type": "object",
            "properties": {
                "param1": {"type": "string"}
            },
            "required": ["param1"]
        }
    }
    
    backend_tool = {
        "name": "test_tool",
        "inputSchema": {
            "type": "object",
            "properties": {
                "param1": {"type": "string"},
                "param2": {"type": "integer"}
            },
            "required": ["param1", "param2"]  # param2 is required in backend
        }
    }
    
    issues = compare_tool_definitions(proxy_tool, backend_tool)
    assert len(issues) > 0, "Expected issues for missing required field"
    assert any("param2" in issue.lower() for issue in issues), "Expected issue about param2"


def test_compare_tool_definitions_type_mismatch():
    """Test that incompatible types are detected."""
    proxy_tool = {
        "name": "test_tool",
        "inputSchema": {
            "type": "object",
            "properties": {
                "param1": {"type": "string"}
            },
            "required": ["param1"]
        }
    }
    
    backend_tool = {
        "name": "test_tool",
        "inputSchema": {
            "type": "object",
            "properties": {
                "param1": {"type": "integer"}  # Type mismatch
            },
            "required": ["param1"]
        }
    }
    
    issues = compare_tool_definitions(proxy_tool, backend_tool)
    assert len(issues) > 0, "Expected issues for type mismatch"


# ============================================================================
# Test is_read_only_tool
# ============================================================================

def test_is_read_only_tool_with_decorator():
    """Test that function decorator is checked first."""
    @read_only
    def test_tool():
        pass
    
    result = is_read_only_tool("test_tool", None, test_tool)
    assert result is True, "Tool with @read_only decorator should be read-only"


def test_is_read_only_tool_with_write_decorator():
    """Test that @write_operation decorator is respected."""
    @write_operation
    def test_tool():
        pass
    
    result = is_read_only_tool("test_tool", None, test_tool)
    assert result is False, "Tool with @write_operation decorator should not be read-only"


def test_is_read_only_tool_with_definition_metadata():
    """Test that tool definition metadata is used."""
    tool_definition = {"readOnly": True}
    result = is_read_only_tool("test_tool", tool_definition, None)
    assert result is True, "Tool with readOnly=True in definition should be read-only"


def test_is_read_only_tool_defaults_to_false():
    """Test that unknown tools default to not read-only (safer)."""
    result = is_read_only_tool("unknown_tool", None, None)
    assert result is False, "Unknown tools should default to not read-only for safety"


# ============================================================================
# Test is_transient_error
# ============================================================================

def test_is_transient_error_connection_error():
    """Test that ConnectionError is identified as transient."""
    error = ConnectionError("Connection failed")
    assert is_transient_error(error) is True


def test_is_transient_error_timeout():
    """Test that TimeoutError is identified as transient."""
    error = TimeoutError("Request timed out")
    assert is_transient_error(error) is True


def test_is_transient_error_other():
    """Test that other errors are not transient."""
    error = ValueError("Invalid value")
    assert is_transient_error(error) is False


# ============================================================================
# Test handle_tool_result
# ============================================================================

@pytest.mark.asyncio
async def test_handle_tool_result_success():
    """Test handling successful tool result."""
    result = {
        "content": [{
            "type": "text",
            "text": json.dumps({"success": True, "data": "test"})
        }]
    }
    
    output = await handle_tool_result("test_tool", result)
    assert output == {"success": True, "data": "test"}


@pytest.mark.asyncio
async def test_handle_tool_result_error():
    """Test handling error result."""
    result = {
        "isError": True,
        "content": [{
            "type": "text",
            "text": json.dumps({"error": "Test error", "code": "test_code"})
        }]
    }
    
    output = await handle_tool_result("test_tool", result)
    assert output.get("isError") is True
    assert "error" in output.get("content", [{}])[0].get("text", "")


@pytest.mark.asyncio
async def test_handle_tool_result_bSuccess_false():
    """Test handling result with bSuccess=False."""
    result = {
        "content": [{
            "type": "text",
            "text": json.dumps({"bSuccess": False, "error": "Operation failed"})
        }]
    }
    
    output = await handle_tool_result("test_tool", result)
    assert output.get("isError") is True


# ============================================================================
# Test create_error_response
# ============================================================================

def test_create_error_response_basic():
    """Test creating basic error response."""
    response = create_error_response("Test error")
    assert response.get("isError") is True
    assert "content" in response
    
    content_text = response["content"][0]["text"]
    error_data = json.loads(content_text)
    assert error_data["error"] == "Test error"


def test_create_error_response_with_code():
    """Test creating error response with error code."""
    response = create_error_response("Test error", "test_code")
    assert response.get("isError") is True
    
    content_text = response["content"][0]["text"]
    error_data = json.loads(content_text)
    assert error_data["error"] == "Test error"
    assert error_data["code"] == "test_code"


# ============================================================================
# Test tool decorators
# ============================================================================

def test_read_only_decorator():
    """Test that @read_only decorator sets attribute."""
    @read_only
    def test_func():
        pass
    
    assert get_tool_read_only_flag(test_func) is True


def test_write_operation_decorator():
    """Test that @write_operation decorator sets attribute."""
    @write_operation
    def test_func():
        pass
    
    assert get_tool_read_only_flag(test_func) is False


def test_decorator_on_undecorated_function():
    """Test that undecorated function returns None."""
    def test_func():
        pass
    
    assert get_tool_read_only_flag(test_func) is None


# ============================================================================
# Test call_tool_with_retry (with mocks)
# ============================================================================

@pytest.mark.asyncio
async def test_call_tool_with_retry_read_only_success():
    """Test retry logic for read-only tool that succeeds."""
    mock_client = AsyncMock()
    mock_client.call_tool = AsyncMock(return_value={"result": {"success": True}})
    
    mock_settings = Mock()
    mock_settings.retry_max_attempts = 3
    mock_settings.retry_initial_delay = 0.1
    mock_settings.retry_max_delay = 1.0
    mock_settings.retry_backoff_factor = 2.0
    
    # Mock read-only tool function
    @read_only
    def mock_tool():
        pass
    
    with patch('unreal_mcp_proxy.tools.is_read_only_tool', return_value=True):
        result = await call_tool_with_retry(
            "test_tool",
            {},
            mock_client,
            mock_settings,
            {},
        )
    
    assert result == {"result": {"success": True}}
    assert mock_client.call_tool.call_count == 1


@pytest.mark.asyncio
async def test_call_tool_with_retry_read_only_retry_success():
    """Test retry logic for read-only tool that succeeds on retry."""
    mock_client = AsyncMock()
    # First call fails, second succeeds
    mock_client.call_tool = AsyncMock(side_effect=[
        ConnectionError("Connection failed"),
        {"result": {"success": True}}
    ])
    
    mock_settings = Mock()
    mock_settings.retry_max_attempts = 3
    mock_settings.retry_initial_delay = 0.1
    mock_settings.retry_max_delay = 1.0
    mock_settings.retry_backoff_factor = 2.0
    
    with patch('unreal_mcp_proxy.tools.is_read_only_tool', return_value=True):
        result = await call_tool_with_retry(
            "test_tool",
            {},
            mock_client,
            mock_settings,
            {},
        )
    
    assert result == {"result": {"success": True}}
    assert mock_client.call_tool.call_count == 2


@pytest.mark.asyncio
async def test_call_tool_with_retry_write_operation_no_retry():
    """Test that write operations are not retried."""
    mock_client = AsyncMock()
    mock_client.call_tool = AsyncMock(side_effect=ConnectionError("Connection failed"))
    
    mock_settings = Mock()
    
    with patch('unreal_mcp_proxy.tools.is_read_only_tool', return_value=False):
        with pytest.raises(ConnectionError):
            await call_tool_with_retry(
                "test_tool",
                {},
                mock_client,
                mock_settings,
                {},
            )
    
    # Should only be called once (no retry)
    assert mock_client.call_tool.call_count == 1


# ============================================================================
# Test Runner
# ============================================================================

def run_unit_tests():
    """Run all unit tests."""
    print("\n" + "=" * 60)
    print("UnrealMCPProxy Unit Tests (with mocks)")
    print("=" * 60)
    
    # Run pytest programmatically
    exit_code = pytest.main([__file__, "-v", "--tb=short"])
    return exit_code == 0


# ============================================================================
# Test server module import and startup
# ============================================================================

def test_server_module_import():
    """Test that the server module can be imported without errors or warnings."""
    import warnings
    import importlib.util
    
    # Capture warnings
    with warnings.catch_warnings(record=True) as w:
        warnings.simplefilter("always")
        
        # Try to import the server module
        server_path = Path(__file__).parent.parent / "src" / "unreal_mcp_proxy" / "server.py"
        spec = importlib.util.spec_from_file_location("unreal_mcp_proxy.server", server_path)
        server_module = importlib.util.module_from_spec(spec)
        
        # Set __package__ to simulate module import
        server_module.__package__ = "unreal_mcp_proxy"
        spec.loader.exec_module(server_module)
        
        # Check for deprecation warnings
        deprecation_warnings = [warning for warning in w if issubclass(warning.category, DeprecationWarning)]
        assert len(deprecation_warnings) == 0, \
            f"Deprecation warnings found during server import: {[str(w.message) for w in deprecation_warnings]}"
        
        # Verify key components are available
        assert hasattr(server_module, 'mcp'), "Server module missing 'mcp' attribute"
        assert hasattr(server_module, 'settings'), "Server module missing 'settings' attribute"
        assert hasattr(server_module, 'unreal_client'), "Server module missing 'unreal_client' attribute"


def test_server_module_import_as_script():
    """Test that the server module can be imported when run as a script (absolute imports)."""
    import warnings
    import importlib.util
    
    # Capture warnings
    with warnings.catch_warnings(record=True) as w:
        warnings.simplefilter("always")
        
        # Simulate running as script (no __package__)
        server_path = Path(__file__).parent.parent / "src" / "unreal_mcp_proxy" / "server.py"
        spec = importlib.util.spec_from_file_location("server", server_path)
        server_module = importlib.util.module_from_spec(spec)
        
        # Don't set __package__ to simulate script execution
        # This should trigger the absolute import path
        spec.loader.exec_module(server_module)
        
        # Check for deprecation warnings
        deprecation_warnings = [warning for warning in w if issubclass(warning.category, DeprecationWarning)]
        assert len(deprecation_warnings) == 0, \
            f"Deprecation warnings found during server script import: {[str(w.message) for w in deprecation_warnings]}"
        
        # Verify key components are available
        assert hasattr(server_module, 'mcp'), "Server module missing 'mcp' attribute"
        assert hasattr(server_module, 'settings'), "Server module missing 'settings' attribute"


if __name__ == "__main__":
    success = run_unit_tests()
    sys.exit(0 if success else 1)

