"""Static prompt definitions for Unreal MCP Server.

These definitions are loaded from a shared JSON file (Resources/prompts.json) that is
used by both the proxy and the backend to ensure consistency.

Prompts are fully statically defined (like tools) - they are just text templates that
format arguments into prompt messages. No backend connection is required to generate prompts.

Path Configuration:
- Default: Relative path ../Resources from UnrealMCPProxy/ directory
- Custom: Set UNREAL_MCP_PROXY_DEFINITIONS_PATH environment variable
  - Absolute path: /path/to/Resources
  - Relative path from plugin root: ../CustomResources
"""

import json
import os
from pathlib import Path
from typing import Dict, Any, List, Optional

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
    # This file is in: UnrealMCPProxy/src/unreal_mcp_proxy/prompt_definitions.py
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


def _get_prompts_json_path() -> Path:
    """Get the path to prompts.json file."""
    return _get_resources_path() / "prompts.json"


def _load_prompts_from_json() -> Dict[str, Dict[str, Any]]:
    """Load prompt definitions from shared JSON file.
    
    Returns:
        Dictionary mapping prompt names to prompt definitions
    """
    prompts_json_path = _get_prompts_json_path()
    try:
        with open(prompts_json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            prompts_list = data.get("prompts", [])
            return {prompt["name"]: prompt for prompt in prompts_list}
    except FileNotFoundError:
        raise FileNotFoundError(
            f"Prompt definitions file not found: {prompts_json_path}\n"
            f"Expected location: {prompts_json_path}\n"
            f"Resources directory: {_get_resources_path()}\n"
            f"Set UNREAL_MCP_PROXY_DEFINITIONS_PATH environment variable to specify custom path."
        )
    except json.JSONDecodeError as e:
        raise ValueError(f"Invalid JSON in prompt definitions file: {e}")


def get_prompt_definitions() -> Dict[str, Dict[str, Any]]:
    """Get all prompt definitions from shared JSON file.
    
    Returns:
        Dictionary mapping prompt names to prompt definitions
    """
    return _load_prompts_from_json()


def get_cached_prompt_definitions() -> List[Dict[str, Any]]:
    """Get cached prompt definitions for offline use.
    
    Returns:
        List of prompt definitions
    """
    return list(get_prompt_definitions().values())


def generate_prompt_messages(prompt_name: str, arguments: Optional[Dict[str, Any]] = None) -> List[Dict[str, Any]]:
    """Generate prompt messages for a given prompt name and arguments.
    
    Loads the prompt template from the shared JSON file and formats it with the provided arguments.
    Prompts are just text templates, so they can be generated fully offline.
    
    Args:
        prompt_name: Name of the prompt
        arguments: Optional arguments for the prompt
    
    Returns:
        List of prompt messages (each with role and content)
    """
    if arguments is None:
        arguments = {}
    
    # Load prompt definition from JSON
    prompts = get_prompt_definitions()
    prompt_def = prompts.get(prompt_name)
    
    if not prompt_def:
        raise ValueError(f"Prompt '{prompt_name}' not found in definitions")
    
    # Get template and format it with arguments
    template = prompt_def.get("template", "")
    
    # Format template with arguments (simple string replacement)
    # Handle missing arguments by leaving placeholders as-is
    try:
        prompt_text = template.format(**arguments)
    except KeyError as e:
        # If an argument is missing, use the placeholder name
        prompt_text = template
    
    messages = [{
        "role": "user",
        "content": {
            "type": "text",
            "text": prompt_text
        }
    }]
    
    return messages

