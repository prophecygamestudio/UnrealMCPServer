"""Blueprint Tools definitions for Unreal MCP Server.

These definitions correspond to FUMCP_BlueprintTools in the backend.
MUST be kept in sync with UnrealMCPServer plugin Blueprint Tools.

See .cursorrules for synchronization requirements.
"""

from typing import Dict, Any
from .tool_definitions_helpers import get_markdown_helpers


def get_blueprint_tools(enable_markdown_export: bool = True) -> Dict[str, Dict[str, Any]]:
    """Get Blueprint Tools definitions.
    
    Args:
        enable_markdown_export: Whether to include markdown export support in descriptions
    
    Returns:
        Dictionary mapping tool names to tool definitions
    """
    tools = {}
    markdown_support, markdown_format = get_markdown_helpers(enable_markdown_export)
    
    # search_blueprints
    tools["search_blueprints"] = {
        "name": "search_blueprints",
        "description": "Search for Blueprint assets based on various criteria including name patterns, parent classes, and package paths. Returns array of Blueprint asset information including paths, names, parent classes, and match details. Use 'name' searchType to find Blueprints by name pattern (e.g., 'BP_Player*'), 'parent_class' to find Blueprints that inherit from a class (e.g., 'Actor', 'Pawn', 'Character'), or 'all' for comprehensive search across all criteria.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "searchType": {
                    "type": "string",
                    "enum": ["name", "parent_class", "all"],
                    "description": "Type of search to perform. 'name': Find Blueprints by name pattern (e.g., 'BP_Player*' finds all Blueprints starting with 'BP_Player'). 'parent_class': Find Blueprints that inherit from a class (e.g., 'Actor', 'Pawn', 'Character'). 'all': Comprehensive search across all criteria."
                },
                "searchTerm": {
                    "type": "string",
                    "description": "Search term to match against. For 'name' type: Blueprint name pattern (e.g., 'BP_Player', 'Enemy'). For 'parent_class' type: Parent class name (e.g., 'Actor', 'Pawn', 'Character'). For 'all' type: Searches both name and parent class."
                },
                "packagePath": {
                    "type": "string",
                    "description": "Optional package path to limit search scope. Examples: '/Game/Blueprints' searches in Blueprints folder, '/Game/Characters' searches in Characters folder. Uses Unreal's path format. If not specified, searches entire project."
                },
                "bRecursive": {
                    "type": "boolean",
                    "description": "Whether to search recursively in subfolders. Defaults to true. Set to false to search only the specified packagePath directory without subdirectories.",
                    "default": True
                },
                "maxResults": {
                    "type": "integer",
                    "description": "Maximum number of results to return. Defaults to 0 (no limit). Use with offset for paging through large result sets. Recommended for large searches to limit response size.",
                    "default": 0
                },
                "offset": {
                    "type": "integer",
                    "description": "Number of results to skip before returning results. Defaults to 0. Use with maxResults for paging: first page uses offset=0, second page uses offset=maxResults, etc.",
                    "default": 0
                }
            },
            "required": ["searchType", "searchTerm"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "results": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "assetPath": {"type": "string"},
                            "assetName": {"type": "string"},
                            "packagePath": {"type": "string"},
                            "parentClass": {"type": "string"},
                            "matches": {"type": "array", "items": {"type": "object"}}
                        }
                    },
                    "description": "Array of matching Blueprint assets"
                },
                "totalResults": {"type": "number", "description": "Number of results in this page"},
                "totalCount": {"type": "number", "description": "Total number of matching results"},
                "offset": {"type": "number", "description": "Offset used for this page"},
                "hasMore": {"type": "boolean", "description": "Whether there are more results available"},
                "searchCriteria": {
                    "type": "object",
                    "description": "The search criteria used",
                    "properties": {
                        "searchType": {"type": "string"},
                        "searchTerm": {"type": "string"},
                        "packagePath": {"type": "string", "description": "Optional package path if specified"},
                        "recursive": {"type": "boolean"}
                    },
                    "required": ["searchType", "searchTerm", "recursive"]
                }
            },
            "required": ["results", "totalResults", "totalCount", "offset", "hasMore", "searchCriteria"]
        }
    }
    
    # export_blueprint_markdown
    export_bp_md_desc = (
        "Export Blueprint asset(s) to markdown format for graph inspection. "
        "This is the recommended method for inspecting Blueprint graph structure, as Blueprint exports are too large to return directly in responses. "
        "The markdown export provides complete Blueprint graph information including nodes, variables, functions, and events. "
        "Files are saved to disk at the specified output folder path. Each Blueprint is exported to a separate markdown file named after the asset. "
        "Returns array of successfully exported file paths. "
    )
    if not enable_markdown_export:
        export_bp_md_desc += "WARNING: BP2AI plugin is not available. Markdown export may not be supported. "
    export_bp_md_desc += (
        "After export, agents should read the markdown file using standard file system tools, then parse and optionally flatten the markdown to understand the graph structure. "
        "The MCP cannot perform the simplification/flattening step - this must be done by the agent."
    )
    
    tools["export_blueprint_markdown"] = {
        "name": "export_blueprint_markdown",
        "description": export_bp_md_desc,
        "inputSchema": {
            "type": "object",
            "properties": {
                "blueprintPaths": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Array of Blueprint object paths to export. Each path should be in format '/Game/Folder/BlueprintName' or '/Game/Folder/BlueprintName.BlueprintName'. Examples: ['/Game/Blueprints/BP_Player.BP_Player', '/Game/Characters/BP_Enemy.BP_Enemy']. All paths must be valid Blueprint assets.",
                    "default": []
                },
                "outputFolder": {
                    "type": "string",
                    "description": "The absolute or relative folder path where exported markdown files should be saved. Examples: 'C:/Exports/Blueprints', './Exports', '/tmp/exports'. The folder will be created if it doesn't exist. Each Blueprint is exported to a separate markdown file named after the asset (e.g., 'BP_Player.md', 'BP_Enemy.md')."
                }
            },
            "required": ["blueprintPaths", "outputFolder"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "bSuccess": {"type": "boolean", "description": "Whether the export operation was successful overall", "default": False},
                "exportedCount": {"type": "number", "description": "Number of Blueprints successfully exported", "default": 0},
                "failedCount": {"type": "number", "description": "Number of Blueprints that failed to export", "default": 0},
                "exportedPaths": {"type": "array", "items": {"type": "string"}, "description": "Array of file paths for successfully exported markdown files", "default": []},
                "failedPaths": {"type": "array", "items": {"type": "string"}, "description": "Array of Blueprint paths that failed to export", "default": []},
                "error": {"type": "string", "description": "Error message if bSuccess is false"}
            },
            "required": ["bSuccess", "exportedCount", "failedCount"]
        }
    }
    
    return tools

