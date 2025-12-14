"""Static tool definitions for Unreal MCP Server.

These definitions are used when the backend is offline to provide tool lists to clients.
MUST be kept in sync with UnrealMCPServer plugin tool definitions.

Tool definitions are organized by domain (Common Tools, Asset Tools, Blueprint Tools)
to match the backend structure. See individual domain files for details.

See .cursorrules for synchronization requirements.
"""

import json
from typing import Dict, Any, Optional

from .tool_definitions_common import get_common_tools
from .tool_definitions_asset import get_asset_tools
from .tool_definitions_blueprint import get_blueprint_tools


def get_tool_definitions(enable_markdown_export: bool = True) -> Dict[str, Dict[str, Any]]:
    """Get all tool definitions with conditional features applied.
    
    Combines tool definitions from all domains (Common, Asset, Blueprint)
    to match the backend structure.
    
    Args:
        enable_markdown_export: Whether to include markdown export support in descriptions
    
    Returns:
        Dictionary mapping tool names to tool definitions
    """
    tools = {}
    
    # Combine all tool definitions from domain-specific modules
    tools.update(get_common_tools(enable_markdown_export))
    tools.update(get_asset_tools(enable_markdown_export))
    tools.update(get_blueprint_tools(enable_markdown_export))
    
    return tools


def compare_tool_definitions(cached_tool: Dict[str, Any], backend_tool: Dict[str, Any]) -> list[str]:
    """Check if proxy tool definition is compatible with backend tool definition.
    
    This function checks compatibility rather than exact matches. The proxy can have
    improved schemas (better descriptions, simplified types, etc.) as long as:
    - All required backend fields are present in proxy
    - Field types are compatible
    - Tool names match
    
    Args:
        cached_tool: Proxy tool definition (can be improved/simplified)
        backend_tool: Tool definition from backend (authoritative for required fields)
    
    Returns:
        List of compatibility issues found (empty if compatible)
    """
    issues = []
    tool_name = backend_tool.get("name", "unknown")
    
    # Compare name (must match)
    if cached_tool.get("name") != backend_tool.get("name"):
        issues.append(f"Name mismatch: proxy='{cached_tool.get('name')}', backend='{backend_tool.get('name')}'")
        return issues  # Can't proceed if names don't match
    
    # Check inputSchema compatibility
    cached_input = cached_tool.get("inputSchema", {})
    backend_input = backend_tool.get("inputSchema", {})
    
    # Check required fields: all backend required fields must be in proxy
    backend_required = backend_input.get("required", [])
    cached_properties = cached_input.get("properties", {})
    
    missing_required = [f for f in backend_required if f not in cached_properties]
    if missing_required:
        issues.append(f"Missing required fields in proxy schema: {', '.join(missing_required)}")
    
    # Check type compatibility for required fields
    backend_properties = backend_input.get("properties", {})
    for field_name in backend_required:
        if field_name in cached_properties and field_name in backend_properties:
            cached_type = cached_properties[field_name].get("type")
            backend_type = backend_properties[field_name].get("type")
            if cached_type and backend_type and cached_type != backend_type:
                # Check if types are compatible (e.g., both are numbers, both are strings)
                compatible_types = [
                    {"number", "integer"},  # number and integer are compatible
                    {"string"},  # strings are always compatible
                    {"boolean"},  # booleans are always compatible
                    {"array"},  # arrays are compatible
                    {"object"},  # objects are compatible
                ]
                types_compatible = any(
                    cached_type in group and backend_type in group
                    for group in compatible_types
                )
                if not types_compatible:
                    issues.append(f"Type mismatch for required field '{field_name}': proxy='{cached_type}', backend='{backend_type}'")
    
    # Note: We don't check for exact schema matches anymore. The proxy can have:
    # - Better descriptions
    # - Simplified types (e.g., string instead of enum with one value)
    # - Additional optional fields (backend will ignore them)
    # - Different default values (proxy applies its own defaults)
    
    return issues

