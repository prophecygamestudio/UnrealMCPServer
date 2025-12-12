"""UnrealMCPProxy - Main FastMCP server implementation."""

from __future__ import annotations

import json
import logging
import os
import sys
import asyncio
from enum import Enum
from pathlib import Path
from typing import Optional, Dict, Any, Callable

# Add src directory to path when running as a script
# This allows absolute imports to work when executed directly
# Check if we're running as a script (__package__ will be None or not set)
try:
    package = __package__
except NameError:
    package = None

if not package:
    # Get the directory containing this file
    current_file = Path(__file__).resolve()
    # Go up to src directory (from src/unreal_mcp_proxy/server.py -> src/)
    # current_file.parent = src/unreal_mcp_proxy/
    # current_file.parent.parent = src/
    src_dir = current_file.parent.parent
    if str(src_dir) not in sys.path:
        sys.path.insert(0, str(src_dir))

from fastmcp import FastMCP
from pydantic_settings import BaseSettings, SettingsConfigDict

# Use absolute imports to work when running as a script
from unreal_mcp_proxy.client.unreal_mcp import UnrealMCPClient, ConnectionState
from unreal_mcp_proxy.tool_definitions import get_tool_definitions, compare_tool_definitions


class MCPTransport(str, Enum):
    """MCP transport types."""
    stdio = "stdio"
    sse = "sse"
    http = "http"


class ServerSettings(BaseSettings):
    """Server configuration settings."""
    model_config = SettingsConfigDict(env_prefix="unreal_mcp_proxy_", env_file=".env", extra='ignore')
    
    # Backend (Unreal MCP Server) settings
    backend_host: str = "localhost"
    backend_port: int = 30069
    backend_timeout: int = 30
    health_check_interval: int = 5
    
    # Proxy server settings
    host: str = "0.0.0.0"
    port: int = 30070
    transport: MCPTransport = MCPTransport.stdio  # Default to STDIO for multiple agent support
    
    # Conditional feature flags
    enable_markdown_export: bool = True
    
    # Logging
    log_level: str = "INFO"
    log_file: Optional[str] = None
    log_format: str = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"


def setup_logging(settings: ServerSettings):
    """Configure logging based on settings."""
    # Convert log_level string to logging level constant
    numeric_level = getattr(logging, settings.log_level.upper(), logging.INFO)
    
    # Create formatter
    formatter = logging.Formatter(settings.log_format)
    
    # Get root logger
    root_logger = logging.getLogger()
    root_logger.setLevel(numeric_level)
    
    # Remove existing handlers to avoid duplicates
    root_logger.handlers.clear()
    
    # Add console handler (always)
    console_handler = logging.StreamHandler()
    console_handler.setLevel(numeric_level)
    console_handler.setFormatter(formatter)
    root_logger.addHandler(console_handler)
    
    # Add file handler if log_file is specified
    if settings.log_file:
        # Ensure parent directory exists
        log_path = Path(settings.log_file)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        
        file_handler = logging.FileHandler(settings.log_file, encoding='utf-8')
        file_handler.setLevel(numeric_level)
        file_handler.setFormatter(formatter)
        root_logger.addHandler(file_handler)
    
    # Create module-specific logger for proxy operations
    proxy_logger = logging.getLogger("unreal_mcp_proxy")
    proxy_logger.setLevel(numeric_level)


# Initialize settings and logging
settings = ServerSettings()
setup_logging(settings)
logger = logging.getLogger(__name__)

# Initialize FastMCP server
mcp = FastMCP("unreal-mcp-proxy")

# Initialize backend client
unreal_client = UnrealMCPClient()

# Cache for tool definitions
_cached_tools: Optional[Dict[str, Dict[str, Any]]] = None

# Registered tool handlers
_registered_tools: Dict[str, Callable] = {}


# FastMCP requires tools to be registered at module level with @mcp.tool() decorator.
# We'll create a generic proxy tool that handles all tool calls, and FastMCP will
# handle the tools/list by returning our cached definitions.

# For now, we'll use a simpler approach: FastMCP's tools/list will be handled
# by returning cached tools, and tools/call will proxy to backend.


async def discover_and_register_tools():
    """Discover tools from backend and register them, comparing with cached definitions.
    
    Note: This is called lazily when async initialization happens, not at module load time.
    """
    global _cached_tools, _registered_tools
    
    logger.info("Starting tool discovery")
    
    # Get cached tool definitions
    cached_tools = get_tool_definitions(enable_markdown_export=settings.enable_markdown_export)
    _cached_tools = cached_tools
    
    # Store tool handlers for reference
    _registered_tools.clear()
    for tool_name, tool_def in cached_tools.items():
        # Store tool definitions for reference
        _registered_tools[tool_name] = tool_def
    
    logger.info(f"Loaded {len(_registered_tools)} tool definitions from cache")
    
    # Register all tools with FastMCP
    register_all_tools()
    
    # Try to get tools from backend and compare
    if unreal_client.state == ConnectionState.ONLINE:
        try:
            response = await unreal_client.get_tools_list()
            
            if "error" in response:
                logger.warning(f"Backend returned error when listing tools: {response['error']}")
                logger.info("Using cached tool definitions (offline mode)")
                return
            
            backend_tools = response.get("result", {}).get("tools", [])
            
            # Compare each backend tool with cached definition
            mismatches_found = 0
            for backend_tool in backend_tools:
                tool_name = backend_tool.get("name")
                if not tool_name:
                    continue
                
                cached_tool = cached_tools.get(tool_name)
                if cached_tool:
                    differences = compare_tool_definitions(cached_tool, backend_tool)
                    if differences:
                        mismatches_found += 1
                        # Log detailed differences for debugging
                        import json
                        # Always log the first mismatch in detail to help debugging
                        if mismatches_found == 1:
                            logger.warning(f"=== DETAILED SCHEMA COMPARISON FOR '{tool_name}' ===")
                            logger.warning(f"Cached inputSchema:\n{json.dumps(cached_tool.get('inputSchema', {}), indent=2, sort_keys=True)}")
                            logger.warning(f"Backend inputSchema:\n{json.dumps(backend_tool.get('inputSchema', {}), indent=2, sort_keys=True)}")
                            logger.warning(f"Cached outputSchema:\n{json.dumps(cached_tool.get('outputSchema', {}), indent=2, sort_keys=True)}")
                            logger.warning(f"Backend outputSchema:\n{json.dumps(backend_tool.get('outputSchema', {}), indent=2, sort_keys=True)}")
                            logger.warning("=" * 60)
                        logger.warning(
                            f"Tool definition mismatch detected for '{tool_name}': "
                            f"Backend definition differs from cached definition. "
                            f"Differences: {', '.join(differences)}. "
                            f"Please update UnrealMCPProxy/src/unreal_mcp_proxy/tool_definitions.py "
                            f"to match the backend definition."
                        )
                else:
                    logger.warning(
                        f"Tool '{tool_name}' found in backend but not in cached definitions. "
                        f"Please add it to tool_definitions.py"
                    )
            
            if mismatches_found == 0:
                logger.info(f"Tool discovery completed: {len(backend_tools)} tools found, no mismatches")
            else:
                logger.warning(f"Tool discovery completed: {len(backend_tools)} tools found, {mismatches_found} mismatches detected")
            
        except Exception as e:
            logger.error(f"Error during tool discovery: {str(e)}", exc_info=True)
            logger.info("Using cached tool definitions due to error")
    else:
        logger.info("Backend is offline, using cached tool definitions")


# FastMCP automatically handles tools/list by collecting all @mcp.tool() decorated functions.
# We'll dynamically create tool functions and register them using FastMCP's add_tool method
# or by using the decorator pattern with dynamic function creation.

# FastMCP requires tools to be registered at module level with @mcp.tool() decorators.
# For a proxy, we'll create example tools that demonstrate the pattern.
# In production, we'd need to dynamically register all tools from cache/backend.

# FastMCP automatically collects @mcp.tool() decorated functions for tools/list.
# For a proxy, we need to ensure tools/list returns our cached definitions.
# FastMCP's architecture makes dynamic tool registration challenging, so we'll
# use a hybrid approach:
# 1. Register a few key tools statically as examples
# 2. Override tools/list behavior to return cached definitions
# 3. Handle tools/call to proxy to backend

# Note: FastMCP may not support overriding tools/list directly.
# We may need to use FastMCP's protocol handlers or extend it.
# For now, we'll register example tools and ensure the proxy logic works.

# Dynamic tool registration
# FastMCP requires tools to be registered with @mcp.tool() decorators.
# FastMCP doesn't support **kwargs, so we need to create functions with explicit parameters.
def create_tool_handler(tool_name: str, tool_def: Dict[str, Any]):
    """Create a tool handler function for a specific tool with explicit parameters.
    
    This function generates tool handlers that:
    1. Accept parameters matching the backend schema (with defaults for optional params)
    2. Apply default values for any missing optional parameters
    3. Explicitly pass ALL parameters (including defaults) to the backend
    
    Architectural Note:
    The proxy is the authoritative source for the user-facing API. It applies defaults
    and always sends complete parameter sets to the backend. The backend's defaults serve
    as a safety net for direct backend calls, but the proxy ensures all values are
    explicitly provided when forwarding requests.
    
    Args:
        tool_name: Name of the tool
        tool_def: Tool definition dictionary
        
    Returns:
        Async function that handles the tool call with explicit parameters
    """
    import types
    from typing import get_type_hints
    
    # Get input schema properties
    input_schema = tool_def.get("inputSchema", {})
    properties = input_schema.get("properties", {})
    required = input_schema.get("required", [])
    
    # Build parameter list and function body
    # Sort parameters: required first, then optional
    param_names = list(properties.keys())
    required_params = [p for p in param_names if p in required]
    optional_params = [p for p in param_names if p not in required]
    sorted_param_names = required_params + optional_params
    
    # Create parameter string for function signature
    default_handlers = []  # Store handlers for complex defaults (initialize before if/else)
    
    if not sorted_param_names:
        # No parameters
        param_str = ""
        call_args = "{}"
    else:
        # Build parameters with Optional types for non-required params
        params = []
        call_dict_items = []
        
        for param_name in sorted_param_names:
            param_schema = properties[param_name]
            param_type = param_schema.get("type", "str")
            # Map JSON schema types to Python types
            type_mapping = {
                "string": "str",
                "integer": "int",
                "number": "float",
                "boolean": "bool",
                "array": "list",
                "object": "Dict[str, Any]"
            }
            python_type = type_mapping.get(param_type, "Any")
            
            # Check if parameter has a default value in the schema
            schema_default = param_schema.get("default")
            
            # Make optional if not required
            if param_name not in required:
                # Use schema default if available, otherwise use None
                if schema_default is not None:
                    # Convert default to Python literal for function signature
                    # Note: For complex types (list, dict), we'll use None and handle default in function body
                    if isinstance(schema_default, str):
                        default_value = f'"{schema_default}"'
                        python_type = python_type  # Keep original type, not Optional
                    elif isinstance(schema_default, bool):
                        default_value = str(schema_default)
                        python_type = python_type  # Keep original type, not Optional
                    elif isinstance(schema_default, (int, float)):
                        default_value = str(schema_default)
                        python_type = python_type  # Keep original type, not Optional
                    elif isinstance(schema_default, list):
                        # For lists, use None as default and handle in function body
                        # This avoids issues with mutable defaults in Python
                        default_value = "None"
                        python_type = f"Optional[{python_type}]"
                        # Store default handler - use Python list literal
                        # Convert list items to proper Python literals
                        list_items = []
                        for item in schema_default:
                            if isinstance(item, str):
                                list_items.append(f'"{item}"')
                            else:
                                list_items.append(str(item))
                        list_literal = "[" + ", ".join(list_items) + "]"
                        default_handlers.append(f"    if {param_name} is None:\n        {param_name} = {list_literal}")
                    elif isinstance(schema_default, dict):
                        # For dicts, use None as default and handle in function body
                        default_value = "None"
                        python_type = f"Optional[{python_type}]"
                        # Store default handler - use json.loads for dict (safer than string formatting)
                        dict_json = json.dumps(schema_default)
                        # Escape single quotes in JSON string for Python string literal
                        dict_json_escaped = dict_json.replace("'", "\\'")
                        default_handlers.append(f"    if {param_name} is None:\n        {param_name} = json.loads('{dict_json_escaped}')")
                    else:
                        default_value = "None"
                        python_type = f"Optional[{python_type}]"
                else:
                    python_type = f"Optional[{python_type}]"
                    default_value = "None"
            else:
                default_value = None
            
            if default_value is not None:
                params.append(f"{param_name}: {python_type} = {default_value}")
            else:
                params.append(f"{param_name}: {python_type}")
            
            # Build dictionary for call_tool
            # IMPORTANT: Include ALL parameters in the call dictionary, even optional ones.
            # This ensures the proxy explicitly passes all values (including defaults) to the backend,
            # making the proxy the authoritative source for the API while keeping backend defaults
            # as a safety net for direct backend calls.
            call_dict_items.append(f'"{param_name}": {param_name}')
        
        param_str = ", ".join(params)
        call_args = "{" + ", ".join(call_dict_items) + "}"
    
    # Create function code with default handlers
    default_handler_code = "\n".join(default_handlers) if default_handlers else ""
    if default_handler_code:
        default_handler_code = "\n" + default_handler_code + "\n"
    
    # Check if we need json import (for dict defaults)
    needs_json_import = any("json.loads" in handler for handler in default_handlers)
    json_import = "    import json\n" if needs_json_import else ""
    
    func_code = f"""
async def {tool_name}({param_str}) -> Dict[str, Any]:
    \"\"\"{tool_def.get("description", f"Tool: {tool_name}")}\"\"\"
{json_import}{default_handler_code}    result = await call_tool("{tool_name}", {call_args})
    # FastMCP expects a return value, not the MCP result format
    # Extract the content from the result
    if result.get("isError"):
        error_content = result.get("content", [{{}}])[0]
        error_text = error_content.get("text", "{{}}")
        try:
            error_data = json.loads(error_text)
            raise ValueError(error_data.get("error", "Unknown error"))
        except json.JSONDecodeError:
            raise ValueError(error_text)
    
    # Parse the content to return structured data
    content = result.get("content", [{{}}])[0]
    if content.get("type") == "text":
        try:
            parsed_result = json.loads(content.get("text", "{{}}"))
            # Check if the backend returned bSuccess: false (backend indicates failure this way)
            if isinstance(parsed_result, dict) and parsed_result.get("bSuccess") is False:
                error_msg = parsed_result.get("error", "Operation failed")
                raise ValueError(error_msg)
            return parsed_result
        except json.JSONDecodeError:
            return {{"text": content.get("text", "")}}
    return result
"""
    
    # Execute the function code
    # Get the module's globals so the function can access module-level names
    import sys
    current_module = sys.modules[__name__]
    module_globals = current_module.__dict__
    
    # Execute in the module's namespace so the function has access to call_tool, json, etc.
    exec(func_code, module_globals)
    
    # Get the function from the module's namespace
    handler = module_globals[tool_name]
    
    return handler


def register_all_tools():
    """Register all tools from cache with FastMCP."""
    global _cached_tools
    
    if _cached_tools is None:
        logger.warning("No cached tools available for registration")
        return
    
    logger.info(f"Registering {len(_cached_tools)} tools with FastMCP")
    
    # Get the current module
    import sys
    current_module = sys.modules[__name__]
    
    for tool_name, tool_def in _cached_tools.items():
        # Create the tool handler function
        # The function signature is generated to match the backend schema exactly,
        # including default values, so FastMCP will generate correct schemas
        handler = create_tool_handler(tool_name, tool_def)
        
        # Register with FastMCP using the tool decorator
        # FastMCP will auto-generate schemas from the function signature
        # Since we've ensured the function signatures match the backend schemas exactly,
        # the generated schemas should match the backend
        decorated_handler = mcp.tool()(handler)
        
        # Store in module namespace so FastMCP can discover it
        # FastMCP collects tools at module level, so we add them to the module's __dict__
        setattr(current_module, tool_name, decorated_handler)
    
    logger.info(f"Successfully registered {len(_cached_tools)} tools")


async def call_tool(tool_name: str, arguments: Dict[str, Any]) -> Dict[str, Any]:
    """Call a tool on the backend server (MCP protocol handler).
    
    Args:
        tool_name: Name of the tool to call
        arguments: Tool arguments
    
    Returns:
        Tool result with content array
    """
    # Ensure async components are initialized (lazy initialization)
    await ensure_async_initialized()
    
    logger.info(f"Tool call requested: {tool_name}")
    
    # Check if tool exists in cache (even if offline)
    if _cached_tools and tool_name not in _cached_tools:
        logger.warning(f"Tool '{tool_name}' not found in cached definitions")
        return {
            "isError": True,
            "content": [{
                "type": "text",
                "text": json.dumps({
                    "error": f"Tool '{tool_name}' not found"
                })
            }]
        }
    
    if unreal_client.state == ConnectionState.OFFLINE:
        logger.warning(f"Backend unavailable for tool call '{tool_name}'")
        return {
            "isError": True,
            "content": [{
                "type": "text",
                "text": json.dumps({
                    "error": "Unreal MCP server is not available. Please ensure Unreal Editor is running and the UnrealMCPServer plugin is enabled."
                })
            }]
        }
    
    try:
        response = await unreal_client.call_tool(tool_name, arguments)
        
        if "error" in response:
            error = response["error"]
            logger.error(
                f"Backend returned error for tool '{tool_name}': "
                f"code={error.get('code')}, message={error.get('message')}"
            )
            return {
                "isError": True,
                "content": [{
                    "type": "text",
                    "text": json.dumps(error)
                }]
            }
        
        # Extract result from response
        result = response.get("result", {})
        logger.info(f"Tool call '{tool_name}' succeeded")
        return result
        
    except ConnectionError as e:
        logger.error(f"Connection error calling tool '{tool_name}': {str(e)}", exc_info=True)
        return {
            "isError": True,
            "content": [{
                "type": "text",
                "text": json.dumps({
                    "error": f"Failed to connect to Unreal MCP server: {str(e)}"
                })
            }]
        }
    except TimeoutError as e:
        logger.warning(f"Timeout calling tool '{tool_name}' (timeout={settings.backend_timeout}s)")
        return {
            "isError": True,
            "content": [{
                "type": "text",
                "text": json.dumps({
                    "error": f"Request to Unreal MCP server timed out: {str(e)}"
                })
            }]
        }
    except Exception as e:
        logger.error(f"Error calling tool '{tool_name}': {str(e)}", exc_info=True)
        return {
            "isError": True,
            "content": [{
                "type": "text",
                "text": json.dumps({
                    "error": f"Failed to call tool: {str(e)}"
                })
            }]
        }


# FastMCP protocol handlers
# Note: FastMCP handles protocol methods automatically. For proxying, we need to register
# all tools with @mcp.tool() and let FastMCP handle tools/list and tools/call automatically.
# Protocol methods like initialize, ping, etc. are handled by FastMCP internally.

# For now, we'll register tools and let FastMCP handle the protocol.
# The proxy behavior will be handled through the tool implementations.
    """Handle initialize MCP method - proxies to backend or returns proxy capabilities."""
    if unreal_client.state == ConnectionState.ONLINE:
        try:
            params = {"protocolVersion": protocolVersion}
            if clientInfo:
                params["clientInfo"] = clientInfo
            if capabilities:
                params["capabilities"] = capabilities
            response = await unreal_client.call_method("initialize", params)
            if "result" in response:
                # Cache server capabilities
                result = response["result"]
                logger.info(f"Initialized with backend: protocolVersion={result.get('protocolVersion')}, server={result.get('serverInfo', {}).get('name')}")
                return result
        except Exception as e:
            logger.warning(f"Failed to initialize with backend: {str(e)}, using proxy capabilities")
    
    # Return proxy capabilities (offline mode)
    return {
        "protocolVersion": protocolVersion,  # Use client's requested version
        "serverInfo": {
            "name": "unreal-mcp-proxy",
            "version": "0.1.0"
        },
        "capabilities": {
            "tools": {
                "listChanged": False,
                "inputSchema": True,
                "outputSchema": True
            },
            "resources": {
                "listChanged": False,
                "subscribe": False
            },
            "prompts": {
                "listChanged": False
            }
        }
    }


# Note: FastMCP handles protocol methods (initialize, ping, tools/list, tools/call, etc.) automatically.
# For tools, we register them with @mcp.tool() and FastMCP handles tools/list and tools/call.
# FastMCP auto-generates schemas from function signatures, so we must ensure our function signatures
# match the backend schemas exactly (including default values) to prevent parameter passing issues.

async def start_server():
    """Start the proxy server (for programmatic use)."""
    # Tools are already registered synchronously at module load time
    # Async initialization will happen lazily on first tool call
    logger.info(f"UnrealMCPProxy server ready (transport={settings.transport.value})")
    return mcp


# Register tools synchronously (before FastMCP starts its event loop)
# This ensures tools are available immediately, even if backend is offline
logger.info("Registering tools synchronously...")
cached_tools = get_tool_definitions(enable_markdown_export=settings.enable_markdown_export)
_cached_tools = cached_tools
register_all_tools()
logger.info(f"Registered {len(_cached_tools)} tools")

# Track if async initialization has been done
_async_initialized = False

async def ensure_async_initialized():
    """Ensure async components are initialized (lazy initialization)."""
    global _async_initialized
    if not _async_initialized:
        try:
            # Check if we're in an event loop (FastMCP's event loop)
            loop = asyncio.get_running_loop()
            logger.info("FastMCP event loop detected: initializing async components")
            await unreal_client.initialize_async()
            await discover_and_register_tools()
            _async_initialized = True
            logger.info("Async initialization complete")
        except RuntimeError:
            # No event loop running yet, will initialize later
            logger.debug("No event loop running yet, will initialize on first async call")

if __name__ == "__main__":
    # Start the MCP server
    # FastMCP will create its own event loop
    # Async initialization (health check) will happen lazily on first tool call
    logger.info(f"Starting UnrealMCPProxy server (transport={settings.transport.value})")
    
    # Prepare run parameters based on transport
    run_kwargs = {"transport": settings.transport.value}
    if settings.transport != MCPTransport.stdio:
        # Only add host/port for non-stdio transports
        run_kwargs["host"] = settings.host
        run_kwargs["port"] = settings.port
        run_kwargs["stateless_http"] = True
    
    mcp.run(**run_kwargs)

