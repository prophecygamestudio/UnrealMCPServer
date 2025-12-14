"""Static resource definitions for Unreal MCP Server.

These definitions are loaded from a shared JSON file (Resources/resources.json) that is
used by both the proxy and the backend to ensure consistency.

IMPORTANT: This module caches RESOURCE METADATA ONLY (definitions, templates, descriptions).
The actual resource CONTENT is never cached - it always comes from the backend when reading
a resource. This allows clients to discover available resources when offline, but content
requires the backend to be online.

Path Configuration:
- Default: Relative path ../Resources from UnrealMCPProxy/ directory
- Custom: Set UNREAL_MCP_PROXY_DEFINITIONS_PATH environment variable
  - Absolute path: /path/to/Resources
  - Relative path from plugin root: ../CustomResources
"""

import json
import os
from pathlib import Path
from typing import Dict, Any, List

# Lazy loading of settings to avoid circular imports
_settings_cache = None

def _get_settings():
    """Lazily load settings to avoid circular import issues."""
    global _settings_cache
    if _settings_cache is None:
        try:
            from .config import ServerSettings
            _settings_cache = ServerSettings()
        except (ImportError, Exception):
            _settings_cache = None
    return _settings_cache


def _get_resources_path() -> Path:
    """Get the path to the Resources directory containing JSON definitions.
    
    Uses environment variable if set, otherwise defaults to relative path
    from plugin root: ../Resources
    
    Returns:
        Path to Resources directory
    """
    # This file is in: UnrealMCPProxy/src/unreal_mcp_proxy/resource_definitions.py
    # Plugin root is: UnrealMCPProxy/ (parent of src/)
    _current_file = Path(__file__).resolve()
    _plugin_root = _current_file.parent.parent.parent  # Go up to UnrealMCPProxy/
    
    # Get definitions path from settings or environment
    settings = _get_settings()
    if settings and settings.definitions_path:
        definitions_path_str = settings.definitions_path
    else:
        definitions_path_str = os.getenv("UNREAL_MCP_PROXY_DEFINITIONS_PATH")
    
    if definitions_path_str:
        # User-specified path (absolute or relative to plugin root)
        definitions_path = Path(definitions_path_str)
        if not definitions_path.is_absolute():
            # Relative path - resolve from plugin root
            definitions_path = _plugin_root.parent / definitions_path
        return definitions_path
    else:
        # Default: relative path ../Resources from plugin root
        return _plugin_root.parent / "Resources"


def _get_resources_json_path() -> Path:
    """Get the path to resources.json file."""
    return _get_resources_path() / "resources.json"


def _load_resources_from_json() -> Dict[str, Any]:
    """Load resource definitions from shared JSON file.
    
    Returns:
        Dictionary with 'resources' and 'resourceTemplates' keys
    """
    resources_json_path = _get_resources_json_path()
    try:
        with open(resources_json_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        raise FileNotFoundError(
            f"Resource definitions file not found: {resources_json_path}\n"
            f"Expected location: {resources_json_path}\n"
            f"Resources directory: {_get_resources_path()}\n"
            f"Set UNREAL_MCP_PROXY_DEFINITIONS_PATH environment variable to specify custom path."
        )
    except json.JSONDecodeError as e:
        raise ValueError(f"Invalid JSON in resource definitions file: {e}")


def get_resource_definitions() -> Dict[str, Dict[str, Any]]:
    """Get all static resource definitions (metadata only, not content).
    
    These are static resources with fixed URIs. Resource templates are handled separately.
    
    Returns:
        Dictionary mapping resource URIs to resource definitions (metadata only)
    """
    data = _load_resources_from_json()
    resources_list = data.get("resources", [])
    # Convert list to dict keyed by URI
    return {resource.get("uri", ""): resource for resource in resources_list if resource.get("uri")}


def get_resource_template_definitions() -> List[Dict[str, Any]]:
    """Get all resource template definitions (metadata only, not content).
    
    Resource templates define URI patterns (e.g., unreal+t3d://{filepath}) that can
    generate dynamic resources. The actual resource content is fetched from the backend
    when a resource is read.
    
    Markdown resource templates are filtered out if markdown export is disabled.
    
    Returns:
        List of resource template definitions (metadata: name, description, uriTemplate, mimeType)
    """
    data = _load_resources_from_json()
    templates = data.get("resourceTemplates", [])
    
    # Filter out markdown resources if markdown export is disabled
    settings = _get_settings()
    if settings and not settings.enable_markdown_export:
        templates = [
            template for template in templates
            if not template.get("uriTemplate", "").startswith("unreal+md://")
        ]
    
    return templates


def get_cached_resource_definitions() -> Dict[str, Any]:
    """Get cached resource definitions for offline use.
    
    NOTE: This caches METADATA ONLY (definitions, templates, descriptions).
    Resource content is never cached and must be fetched from the backend.
    
    Returns:
        Dictionary with resources and resourceTemplates lists (metadata only)
    """
    return {
        "resources": list(get_resource_definitions().values()),
        "resourceTemplates": get_resource_template_definitions()
    }

