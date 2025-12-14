"""Tool handling and execution logic for UnrealMCPProxy."""

import asyncio
import json
import logging
from typing import Dict, Any, Optional

from .client.unreal_mcp import UnrealMCPClient, ConnectionState
from .errors import create_error_response
from .config import ServerSettings

logger = logging.getLogger(__name__)


def is_read_only_tool(
    tool_name: str,
    tool_definition: Optional[Dict[str, Any]] = None,
    tool_function: Optional[Any] = None
) -> bool:
    """Check if a tool is read-only and safe to retry.
    
    Checks in this order:
    1. Function decorator (if function provided) - most explicit, set at definition time
    2. Tool definition metadata (if provided) - from proxy definitions
    3. Backend tool definition (if provided) - future-proofing
    
    If none of these provide a definitive answer, defaults to False (not safe to retry)
    for safety.
    
    Args:
        tool_name: Name of the tool
        tool_definition: Optional tool definition from proxy (may contain readOnly field)
        tool_function: Optional tool function object (may have decorator metadata)
    
    Returns:
        True if tool is read-only and safe to retry, False otherwise
    """
    # First check function decorator (most explicit, set at definition time)
    if tool_function is not None:
        from .tool_decorators import get_tool_read_only_flag
        read_only_flag = get_tool_read_only_flag(tool_function)
        if read_only_flag is not None:
            return read_only_flag
    
    # Check proxy tool definition metadata
    if tool_definition and "readOnly" in tool_definition:
        return tool_definition["readOnly"]
    
    # Check backend tool definition (future-proofing)
    # Note: tool_definition here is from proxy, backend would be separate param if needed
    
    # Default to not retrying unknown tools (safer - prevents accidental retries of write operations)
    logger.warning(
        f"Tool '{tool_name}' has no read-only decorator or metadata. "
        f"Defaulting to no retry for safety. Add @read_only or @write_operation decorator to the tool function."
    )
    return False


def is_transient_error(error: Exception) -> bool:
    """Check if an error is transient and worth retrying.
    
    Args:
        error: The exception that occurred
    
    Returns:
        True if error is transient (network, timeout), False otherwise
    """
    return isinstance(error, (ConnectionError, TimeoutError))


async def call_tool_with_retry(
    tool_name: str,
    arguments: Dict[str, Any],
    client: UnrealMCPClient,
    settings: ServerSettings,
    cached_proxy_tool_definitions: Optional[Dict[str, Dict[str, Any]]],
    max_retries: Optional[int] = None,
    initial_delay: Optional[float] = None,
    max_delay: Optional[float] = None,
    backoff_factor: Optional[float] = None
) -> Dict[str, Any]:
    """Call a tool with exponential backoff retry for read-only operations.
    
    Args:
        tool_name: Name of the tool to call
        arguments: Tool arguments
        client: UnrealMCPClient instance
        settings: ServerSettings instance
        cached_proxy_tool_definitions: Dictionary of proxy tool definitions
        max_retries: Maximum number of retry attempts (default: from settings)
        initial_delay: Initial delay before first retry in seconds (default: from settings)
        max_delay: Maximum delay between retries in seconds (default: from settings)
        backoff_factor: Multiplier for exponential backoff (default: from settings)
    
    Returns:
        Tool result with content array
    """
    # Use settings defaults if not provided
    max_retries = max_retries if max_retries is not None else settings.retry_max_attempts
    initial_delay = initial_delay if initial_delay is not None else settings.retry_initial_delay
    max_delay = max_delay if max_delay is not None else settings.retry_max_delay
    backoff_factor = backoff_factor if backoff_factor is not None else settings.retry_backoff_factor
    
    # Check if tool is read-only (safe to retry)
    # Try to get the function from server module to check decorator
    # Use importlib to avoid circular import issues
    tool_function = None
    try:
        import importlib
        server_module = importlib.import_module('unreal_mcp_proxy.server')
        tool_function = getattr(server_module, tool_name, None)
    except (ImportError, AttributeError):
        pass
    
    proxy_tool_definition = cached_proxy_tool_definitions.get(tool_name) if cached_proxy_tool_definitions else None
    is_read_only = is_read_only_tool(tool_name, proxy_tool_definition, tool_function)
    
    if not is_read_only:
        # For write operations, call once without retry
        logger.debug(f"Tool '{tool_name}' is a write operation - no retry on errors")
        return await client.call_tool(tool_name, arguments)
    
    # For read-only operations, retry on transient errors
    last_exception = None
    delay = initial_delay
    
    for attempt in range(max_retries + 1):  # +1 for initial attempt
        try:
            response = await client.call_tool(tool_name, arguments)
            # Success - return immediately
            if attempt > 0:
                logger.info(f"Tool '{tool_name}' succeeded on retry attempt {attempt + 1}")
            return response
            
        except (ConnectionError, TimeoutError) as e:
            last_exception = e
            
            if attempt < max_retries:
                # Calculate next delay with exponential backoff
                wait_time = min(delay, max_delay)
                logger.warning(
                    f"Transient error calling tool '{tool_name}' (attempt {attempt + 1}/{max_retries + 1}): {str(e)}. "
                    f"Retrying in {wait_time:.2f}s..."
                )
                await asyncio.sleep(wait_time)
                delay *= backoff_factor
            else:
                # Out of retries
                logger.error(
                    f"Tool '{tool_name}' failed after {max_retries + 1} attempts: {str(e)}"
                )
                raise
        
        except Exception as e:
            # Non-transient error - don't retry
            logger.error(f"Non-transient error calling tool '{tool_name}': {str(e)}", exc_info=True)
            raise
    
    # Should never reach here, but just in case
    if last_exception:
        raise last_exception
    raise RuntimeError(f"Unexpected retry loop exit for tool '{tool_name}'")


async def handle_tool_result(tool_name: str, result: Dict[str, Any]) -> Dict[str, Any]:
    """Handle tool result from backend, converting MCP format to return value.
    
    Returns error dictionaries (with isError: True) instead of raising exceptions
    to maintain consistency with FastMCP's expected return format.
    
    Args:
        tool_name: Name of the tool that was called
        result: Result dictionary from backend
    
    Returns:
        Processed result dictionary
    """
    if result.get("isError"):
        error_content = result.get("content", [{}])[0]
        error_text = error_content.get("text", "{}")
        try:
            error_data = json.loads(error_text)
            error_message = error_data.get("error", "Unknown error")
            error_code = error_data.get("code")
            return create_error_response(error_message, error_code)
        except json.JSONDecodeError:
            return create_error_response(error_text)
    
    content = result.get("content", [{}])[0]
    if content.get("type") == "text":
        try:
            parsed_result = json.loads(content.get("text", "{}"))
            if isinstance(parsed_result, dict) and parsed_result.get("bSuccess") is False:
                error_msg = parsed_result.get("error", "Operation failed")
                return create_error_response(error_msg)
            return parsed_result
        except json.JSONDecodeError:
            return {"text": content.get("text", "")}
    return result


async def call_tool(
    tool_name: str,
    arguments: Dict[str, Any],
    client: UnrealMCPClient,
    settings: ServerSettings,
    cached_proxy_tool_definitions: Optional[Dict[str, Dict[str, Any]]],
    check_compatibility_when_online: Optional[callable] = None
) -> Dict[str, Any]:
    """Call a tool on the backend server (MCP protocol handler).
    
    Args:
        tool_name: Name of the tool to call
        arguments: Tool arguments
        client: UnrealMCPClient instance
        settings: ServerSettings instance
        cached_proxy_tool_definitions: Dictionary of proxy tool definitions
        check_compatibility_when_online: Optional async function to check compatibility when backend comes online
    
    Returns:
        Tool result with content array
    """
    logger.info(f"Tool call requested: {tool_name}")
    
    # Lazy initialization: start health check and compatibility checking if not already started
    # This ensures initialization happens on first tool call if it wasn't done at startup
    # Can be disabled via settings.health_check_start_on_first_call = False
    if settings.health_check_start_on_first_call and not client._health_check_started:
        try:
            await client.initialize_async()
            # Check compatibility when backend comes online
            if check_compatibility_when_online:
                asyncio.create_task(check_compatibility_when_online())
        except RuntimeError:
            # No event loop yet, will start later
            logger.debug("No event loop available - initialization deferred")
    
    # Check if tool exists in proxy tool definitions (even if offline)
    if cached_proxy_tool_definitions and tool_name not in cached_proxy_tool_definitions:
        logger.warning(f"Tool '{tool_name}' not found in proxy tool definitions")
        return create_error_response(f"Tool '{tool_name}' not found")
    
    if client.state == ConnectionState.OFFLINE:
        logger.warning(f"Backend unavailable for tool call '{tool_name}'")
        return create_error_response(
            "Unreal MCP server is not available. Please ensure Unreal Editor is running and the UnrealMCPServer plugin is enabled."
        )
    
    try:
        # Use retry logic for read-only operations
        response = await call_tool_with_retry(
            tool_name,
            arguments,
            client,
            settings,
            cached_proxy_tool_definitions
        )
        
        if "error" in response:
            error = response["error"]
            error_code = error.get("code")
            error_message = error.get("message", str(error))
            logger.error(
                f"Backend returned error for tool '{tool_name}': "
                f"code={error_code}, message={error_message}"
            )
            return create_error_response(error_message, str(error_code) if error_code else None)
        
        # Extract result from response
        result = response.get("result", {})
        logger.info(f"Tool call '{tool_name}' succeeded")
        return result
        
    except ConnectionError as e:
        logger.error(f"Connection error calling tool '{tool_name}': {str(e)}", exc_info=True)
        return create_error_response(f"Failed to connect to Unreal MCP server: {str(e)}", "connection_error")
    except TimeoutError as e:
        logger.warning(f"Timeout calling tool '{tool_name}' (timeout={settings.backend_timeout}s)")
        return create_error_response(f"Request to Unreal MCP server timed out: {str(e)}", "timeout_error")
    except Exception as e:
        logger.error(f"Error calling tool '{tool_name}': {str(e)}", exc_info=True)
        return create_error_response(f"Failed to call tool: {str(e)}", "internal_error")

