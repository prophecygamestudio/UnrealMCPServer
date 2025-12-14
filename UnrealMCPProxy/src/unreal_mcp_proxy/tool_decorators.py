"""Decorators for marking tool functions with metadata (read-only vs write operations)."""

from functools import wraps
from typing import Callable, Any


def read_only(func: Callable) -> Callable:
    """Decorator to mark a tool function as read-only (safe to retry).
    
    Read-only operations are safe to retry on transient errors because they
    don't modify state. Examples: queries, searches, exports.
    
    Usage:
        @mcp.tool()
        @read_only
        async def get_project_config() -> Dict[str, Any]:
            ...
    """
    func.__tool_read_only__ = True
    return func


def write_operation(func: Callable) -> Callable:
    """Decorator to mark a tool function as a write operation (not safe to retry).
    
    Write operations modify state and should NOT be retried automatically.
    Examples: console commands, imports, compilation requests.
    
    Usage:
        @mcp.tool()
        @write_operation
        async def execute_console_command(command: str) -> Dict[str, Any]:
            ...
    """
    func.__tool_read_only__ = False
    return func


def get_tool_read_only_flag(func: Callable) -> bool | None:
    """Get the read-only flag from a tool function if it exists.
    
    Args:
        func: The tool function
    
    Returns:
        True if read-only, False if write operation, None if not marked
    """
    return getattr(func, '__tool_read_only__', None)

