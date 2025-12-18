"""Asset Tools definitions for Unreal MCP Server.

These definitions correspond to FUMCP_AssetTools in the backend.
MUST be kept in sync with UnrealMCPServer plugin Asset Tools.

See .cursorrules for synchronization requirements.
"""

from typing import Dict, Any
from .tool_definitions_helpers import get_markdown_helpers


def get_asset_tools(enable_markdown_export: bool = True) -> Dict[str, Dict[str, Any]]:
    """Get Asset Tools definitions.
    
    Args:
        enable_markdown_export: Whether to include markdown export support in descriptions
    
    Returns:
        Dictionary mapping tool names to tool definitions
    """
    tools = {}
    markdown_support, markdown_format = get_markdown_helpers(enable_markdown_export)
    
    # export_asset (with conditional markdown support)
    export_asset_desc = (
        "Export a single UObject to a specified format (defaults to T3D). "
        "Exportable asset types include: StaticMesh, Texture2D, Material, Sound, Animation, and most UObject-derived classes. "
        "Returns the exported content as a string. T3D format provides human-readable text representation of Unreal objects. "
    )
    if enable_markdown_export:
        export_asset_desc += markdown_support
    export_asset_desc += (
        "IMPORTANT: This tool will fail if used with Blueprint assets. "
        "Blueprints must be exported using batch_export_assets instead, as Blueprint exports generate responses too large to be parsed. "
        "For large exports, consider using batch_export_assets which saves to files."
    )
    
    format_desc = "The export format. 'T3D': Human-readable text representation (default). "
    if enable_markdown_export:
        format_desc += "'md': Structured markdown documentation. "
    format_desc += "Defaults to 'T3D' if not specified. Other formats may be available depending on the asset type (e.g., 'OBJ' for meshes)."
    
    tools["export_asset"] = {
        "name": "export_asset",
        "description": export_asset_desc,
        "inputSchema": {
            "type": "object",
            "properties": {
                "objectPath": {
                    "type": "string",
                    "description": "The Unreal Engine object path to export. Format: '/Game/Folder/AssetName' or '/Game/Folder/AssetName.AssetName'. Examples: '/Game/MyAsset', '/Game/Textures/MyTexture.MyTexture', '/Engine/EditorMaterials/GridMaterial'. Blueprint assets are not supported and will fail. Use batch_export_assets for Blueprint assets. Use query_asset first to verify the asset exists."
                },
                "format": {
                    "type": "string",
                    "description": format_desc,
                    "default": "T3D"
                }
            },
            "required": ["objectPath"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "bSuccess": {"type": "boolean", "description": "Whether the export was successful", "default": False},
                "objectPath": {"type": "string", "description": "The path to the object that was exported"},
                "format": {"type": "string", "description": f"The export format used (e.g., 'T3D'{markdown_format}, 'OBJ')"},
                "content": {"type": "string", "description": "The exported asset content in the specified format. The format varies depending on the exporter type and object type."},
                "error": {"type": "string", "description": "Error message if bSuccess is false"}
            },
            "required": ["bSuccess", "objectPath"]
        }
    }
    
    # query_asset
    tools["query_asset"] = {
        "name": "query_asset",
        "description": "Query a single asset to check if it exists and get its basic information from the asset registry. Use this before export_asset or import_asset to verify an asset exists. Faster than export_asset for simple existence checks. Returns asset path, name, class, package path, and optionally tags. Returns error if asset doesn't exist.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "Single asset path to query. Format: '/Game/MyAsset' or '/Game/MyFolder/MyAsset' or '/Game/MyFolder/MyAsset.MyAsset'. Examples: '/Game/MyAsset', '/Game/Blueprints/BP_Player', '/Engine/EditorMaterials/GridMaterial'. Must start with '/Game/' or '/Engine/'. Asset must exist in the project."
                },
                "bIncludeTags": {
                    "type": "boolean",
                    "description": "Whether to include asset tags in the response. Defaults to false. Set to true to get additional metadata tags associated with the asset (e.g., 'ParentClass' for Blueprints, 'TextureGroup' for textures).",
                    "default": False
                }
            },
            "required": ["assetPath"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "bExists": {"type": "boolean", "description": "Whether the asset exists", "default": False},
                "assetPath": {"type": "string", "description": "The asset path that was queried"},
                "assetName": {"type": "string", "description": "Name of the asset (if bExists is true)"},
                "packagePath": {"type": "string", "description": "Package path of the asset (if bExists is true)"},
                "classPath": {"type": "string", "description": "Class path of the asset (if bExists is true)"},
                "objectPath": {"type": "string", "description": "Full object path of the asset (if bExists is true)"},
                "tags": {"type": "object", "additionalProperties": {"type": "string"}, "description": "Asset tags (if bIncludeTags was true and bExists is true)", "default": {}}
            },
            "required": ["bExists", "assetPath"]
        }
    }
    
    # batch_export_assets (with conditional markdown support)
    batch_export_desc = (
        "Export multiple assets to files in a specified folder. Returns a list of the exported file paths. "
        "Required for Blueprint assets, as export_asset will fail for Blueprints due to response size limitations. "
        "Use this when exporting multiple assets of any type. Files are saved to disk at the specified output folder path. "
        "Format defaults to T3D. Each asset is exported to a separate file named after the asset. "
        "Returns array of successfully exported file paths. Failed exports are not included in the return value. "
        "NOTE: For Blueprint graph inspection, use export_blueprint_markdown instead, which is specifically designed for that purpose and provides clearer workflow guidance."
    )
    
    batch_format_desc = "The export format. Defaults to 'T3D' if not specified. Examples: 'T3D' (human-readable text), 'OBJ' (for meshes). "
    if enable_markdown_export:
        batch_format_desc += "'md' (markdown): Available for assets that support markdown export. "
    batch_format_desc += "Format must be supported by the exporter for each asset type. NOTE: For Blueprint markdown export, use export_blueprint_markdown instead."
    
    tools["batch_export_assets"] = {
        "name": "batch_export_assets",
        "description": batch_export_desc,
        "inputSchema": {
            "type": "object",
            "properties": {
                "objectPaths": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Array of Unreal Engine object paths to export. Each path should be in format '/Game/Folder/AssetName' or '/Game/Folder/AssetName.AssetName'. Examples: ['/Game/MyAsset', '/Game/Blueprints/BP_Player.BP_Player']. Can include Blueprint assets (unlike export_asset).",
                    "default": []
                },
                "outputFolder": {
                    "type": "string",
                    "description": "The absolute or relative folder path where exported files should be saved. Examples: 'C:/Exports/Blueprints', './Exports', '/tmp/exports'. The folder will be created if it doesn't exist. Each asset is exported to a separate file named after the asset (e.g., 'BP_Player.T3D', 'MyTexture.T3D', 'BP_Player.md' for markdown format)."
                },
                "format": {
                    "type": "string",
                    "description": batch_format_desc,
                    "default": "T3D"
                }
            },
            "required": ["objectPaths", "outputFolder"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "bSuccess": {"type": "boolean", "description": "Whether the batch export operation was successful overall", "default": False},
                "exportedCount": {"type": "number", "description": "Number of assets successfully exported", "default": 0},
                "failedCount": {"type": "number", "description": "Number of assets that failed to export", "default": 0},
                "exportedPaths": {"type": "array", "items": {"type": "string"}, "description": "Array of file paths for successfully exported assets", "default": []},
                "failedPaths": {"type": "array", "items": {"type": "string"}, "description": "Array of object paths that failed to export", "default": []},
                "error": {"type": "string", "description": "Error message if bSuccess is false"}
            },
            "required": ["bSuccess", "exportedCount", "failedCount"]
        }
    }
    
    # export_class_default
    tools["export_class_default"] = {
        "name": "export_class_default",
        "description": "Export the class default object (CDO) for a given class path. This allows determining default values for a class, since exporting instances of objects do not print values that are identical to the default value. Use this to understand default property values for Unreal classes. Useful for comparing instance values against defaults. Returns T3D format by default, showing all default property values for the class.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "classPath": {
                    "type": "string",
                    "description": "The class path to export the default object for. C++ class format: '/Script/Engine.Actor', '/Script/Engine.Pawn', '/Script/Engine.Character'. Blueprint class format: '/Game/Blueprints/BP_Player.BP_Player_C' (note the '_C' suffix for Blueprint classes). Examples: '/Script/Engine.Actor', '/Script/Engine.Texture2D', '/Game/Blueprints/BP_Enemy.BP_Enemy_C'."
                },
                "format": {
                    "type": "string",
                    "description": "The export format. Defaults to 'T3D' if not specified. 'T3D' provides human-readable text showing all default property values. Other formats may be available depending on the class type.",
                    "default": "T3D"
                }
            },
            "required": ["classPath"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "bSuccess": {"type": "boolean", "description": "Whether the export was successful", "default": False},
                "classPath": {"type": "string", "description": "The class path that was exported"},
                "format": {"type": "string", "description": "The export format used (e.g., 'T3D', 'OBJ')"},
                "content": {"type": "string", "description": "The exported class default object content in the specified format."},
                "error": {"type": "string", "description": "Error message if bSuccess is false"}
            },
            "required": ["bSuccess", "classPath"]
        }
    }
    
    # import_asset
    tools["import_asset"] = {
        "name": "import_asset",
        "description": "Import a file to create or update a UObject. The file type is automatically detected based on available factories. Import binary files (textures, meshes, sounds) or T3D files to create/update Unreal assets. Supported binary formats: .fbx, .obj (meshes), .png, .jpg, .tga (textures), .wav, .mp3 (sounds). T3D files can be used to import from T3D format or to configure imported objects. If asset exists at packagePath, it will be updated. Otherwise, a new asset is created. At least one of filePath (binary) or t3dFilePath (T3D) must be provided.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "filePath": {
                    "type": "string",
                    "description": "The absolute or relative path to the binary file to import. Supported formats: .fbx, .obj (meshes), .png, .jpg, .tga (textures), .wav, .mp3 (sounds). Examples: 'C:/Models/MyMesh.fbx', './Textures/MyTexture.png'. Optional if t3dFilePath is provided. At least one of filePath or t3dFilePath must be specified."
                },
                "t3dFilePath": {
                    "type": "string",
                    "description": "The absolute or relative path to the T3D file for configuration. T3D files can be used to import from T3D format or to configure imported objects. Examples: 'C:/Exports/MyAsset.T3D', './Config/MyAsset.T3D'. Optional if filePath is provided. At least one of filePath or t3dFilePath must be specified."
                },
                "packagePath": {
                    "type": "string",
                    "description": "The full object path where the object should be created, including the object name. Format: '/Game/MyFolder/MyAsset.MyAsset' (include object name after the dot). Examples: '/Game/MyAsset.MyAsset', '/Game/Textures/MyTexture.MyTexture', '/Game/Meshes/MyMesh.MyMesh'. If asset exists at this path, it will be updated. Otherwise, a new asset is created."
                },
                "classPath": {
                    "type": "string",
                    "description": "The class path of the object to import. C++ class format: '/Script/Engine.Texture2D', '/Script/Engine.StaticMesh', '/Script/Engine.SoundWave'. Blueprint class format: '/Game/Blueprints/BP_Player.BP_Player_C'. Examples: '/Script/Engine.Texture2D' (for textures), '/Script/Engine.StaticMesh' (for meshes), '/Script/Engine.SoundWave' (for sounds)."
                }
            },
            "required": ["packagePath", "classPath"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "bSuccess": {"type": "boolean", "description": "Whether the import was successful", "default": False},
                "count": {"type": "number", "description": "Number of objects imported (if bSuccess is true)", "default": 0},
                "filePath": {"type": "string", "description": "The absolute file path that was imported"},
                "packagePath": {"type": "string", "description": "The package path where objects were imported"},
                "factoryClass": {"type": "string", "description": "The factory class name used for import"},
                "importedObjects": {"type": "array", "items": {"type": "string"}, "description": "Array of object paths for imported objects (if bSuccess is true)", "default": []},
                "error": {"type": "string", "description": "Error message if bSuccess is false"}
            },
            "required": ["bSuccess"]
        }
    }
    
    # search_assets
    tools["search_assets"] = {
        "name": "search_assets",
        "description": "Search for assets by package paths or package names, optionally filtered by class. Returns an array of asset information from the asset registry. More flexible than search_blueprints as it works with all asset types. REQUIRED: At least one of 'packagePaths' or 'packageNames' must be provided (non-empty array). Use packagePaths to search directories (e.g., '/Game/Blueprints' searches all assets in that folder), packageNames for exact or partial package matches (supports wildcards * and ?, or substring matching), and classPaths to filter by asset type (e.g., textures only). Returns array of asset information. Use bIncludeTags=true to get additional metadata tags. Use maxResults and offset for paging through large result sets. For large searches, use maxResults to limit results and offset for paging.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "packagePaths": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "REQUIRED (if packageNames is empty): Array of directory/package paths to search for assets. Examples: ['/Game/Blueprints', '/Game/Materials', '/Game/Textures']. Uses Unreal's path format. Searches all assets in specified folders (recursive by default). At least one of packagePaths or packageNames must be provided (non-empty array). For large directories, use maxResults and offset for paging.",
                    "default": []
                },
                "packageNames": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "REQUIRED (if packagePaths is empty): Array of package names to search for. Supports both exact matches and partial matches. Examples: ['MyAsset', '/Game/MyAsset', '/Game/Blueprints/BP_Player'] for exact matches, ['BP_*', '*Player*', 'MyAsset'] for partial matches. Partial matching supports: (1) Wildcards: * (matches any characters) and ? (matches single character), e.g., 'BP_*' matches all packages starting with 'BP_'; (2) Substring matching: partial names without wildcards will match if the package name contains the substring (case-insensitive), e.g., 'Player' matches '/Game/Blueprints/BP_Player'. Can be used instead of or in combination with packagePaths. At least one of packagePaths or packageNames must be provided (non-empty array). More targeted than packagePaths as it searches for specific packages.",
                    "default": []
                },
                "classPaths": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Array of class paths to filter by. Examples: ['/Script/Engine.Blueprint', '/Script/Engine.Texture2D', '/Script/Engine.StaticMesh']. If empty, searches all asset types. C++ classes: '/Script/Engine.ClassName'. Blueprint classes: '/Game/Blueprints/BP_Player.BP_Player_C'. Recommended for large searches to reduce result set size.",
                    "default": []
                },
                "bRecursive": {
                    "type": "boolean",
                    "description": "Whether to search recursively in subdirectories. Defaults to true. Set to false to search only the specified packagePaths directories without subdirectories.",
                    "default": True
                },
                "bIncludeTags": {
                    "type": "boolean",
                    "description": "Whether to include asset tags in the response. Defaults to false. Set to true to get additional metadata tags for each asset (e.g., 'ParentClass' for Blueprints, 'TextureGroup' for textures, 'AssetImportData' for imported assets).",
                    "default": False
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
            "required": [
                "packagePaths",
                "packageNames",
                "classPaths",
                "bRecursive",
                "bIncludeTags"
            ]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "assets": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "exists": {"type": "boolean"},
                            "assetPath": {"type": "string"},
                            "assetName": {"type": "string"},
                            "packagePath": {"type": "string"},
                            "classPath": {"type": "string"},
                            "objectPath": {"type": "string"},
                            "tags": {"type": "object", "additionalProperties": {"type": "string"}}
                        }
                    },
                    "description": "Array of asset information objects"
                },
                "count": {"type": "integer", "description": "Number of assets returned in this page"},
                "totalCount": {"type": "integer", "description": "Total number of assets found (before paging)"},
                "offset": {"type": "integer", "description": "Offset used for this page"},
                "hasMore": {"type": "boolean", "description": "Whether there are more results available (true if offset + count < totalCount)"}
            },
            "required": ["assets", "count", "totalCount", "offset", "hasMore"]
        }
    }
    
    # get_asset_dependencies
    tools["get_asset_dependencies"] = {
        "name": "get_asset_dependencies",
        "description": "Get all assets that a specified asset depends on. Returns an array of asset paths that the specified asset depends on. Use this to understand what assets an asset requires, which is useful for impact analysis, refactoring safety, and understanding asset relationships. Very useful when doing asset searches and queries with existing tools. Supports both hard dependencies (direct references) and soft dependencies (searchable references).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "The asset path to get dependencies for. Format: '/Game/Folder/AssetName' or '/Game/Folder/AssetName.AssetName'. Examples: '/Game/MyAsset', '/Game/Blueprints/BP_Player', '/Engine/EditorMaterials/GridMaterial'. Must start with '/Game/' or '/Engine/'. Asset must exist in the project."
                },
                "bIncludeHardDependencies": {
                    "type": "boolean",
                    "description": "Whether to include hard dependencies (direct references). Defaults to true. Hard dependencies are assets that are directly referenced by the asset.",
                    "default": True
                },
                "bIncludeSoftDependencies": {
                    "type": "boolean",
                    "description": "Whether to include soft dependencies (searchable references). Defaults to false. Soft dependencies are assets that are referenced via searchable references (e.g., string-based asset references).",
                    "default": False
                }
            },
            "required": ["assetPath"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "bSuccess": {"type": "boolean", "description": "Whether the operation completed successfully", "default": False},
                "assetPath": {"type": "string", "description": "The asset path that was queried"},
                "dependencies": {"type": "array", "items": {"type": "string"}, "description": "Array of asset paths that this asset depends on", "default": []},
                "count": {"type": "number", "description": "Number of dependencies found", "default": 0},
                "error": {"type": "string", "description": "Error message if bSuccess is false"}
            },
            "required": ["bSuccess", "assetPath", "dependencies", "count"]
        }
    }
    
    # get_asset_references
    tools["get_asset_references"] = {
        "name": "get_asset_references",
        "description": "Get all assets that reference a specified asset. Returns an array of asset paths that reference the specified asset. Use this to understand what assets depend on this asset, which is critical for impact analysis, refactoring safety, and unused asset detection. Very useful when doing asset searches and queries with existing tools. Supports both hard references (direct references) and soft references (searchable references).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "The asset path to get references for. Format: '/Game/Folder/AssetName' or '/Game/Folder/AssetName.AssetName'. Examples: '/Game/MyAsset', '/Game/Blueprints/BP_Player', '/Engine/EditorMaterials/GridMaterial'. Must start with '/Game/' or '/Engine/'. Asset must exist in the project."
                },
                "bIncludeHardReferences": {
                    "type": "boolean",
                    "description": "Whether to include hard references (direct references). Defaults to true. Hard references are assets that directly reference this asset.",
                    "default": True
                },
                "bIncludeSoftReferences": {
                    "type": "boolean",
                    "description": "Whether to include soft references (searchable references). Defaults to false. Soft references are assets that reference this asset via searchable references (e.g., string-based asset references).",
                    "default": False
                }
            },
            "required": ["assetPath"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "bSuccess": {"type": "boolean", "description": "Whether the operation completed successfully", "default": False},
                "assetPath": {"type": "string", "description": "The asset path that was queried"},
                "references": {"type": "array", "items": {"type": "string"}, "description": "Array of asset paths that reference this asset", "default": []},
                "count": {"type": "number", "description": "Number of references found", "default": 0},
                "error": {"type": "string", "description": "Error message if bSuccess is false"}
            },
            "required": ["bSuccess", "assetPath", "references", "count"]
        }
    }
    
    # get_asset_dependency_tree
    tools["get_asset_dependency_tree"] = {
        "name": "get_asset_dependency_tree",
        "description": "Get the complete dependency tree for a specified asset. Returns a recursive tree structure showing all dependencies and their dependencies. Use this for complete dependency mapping and recursive analysis. The tree includes depth information for each node. Very useful when doing asset searches and queries with existing tools. Supports both hard dependencies (direct references) and soft dependencies (searchable references). Use maxDepth to limit recursion depth and prevent infinite loops.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "The asset path to get dependency tree for. Format: '/Game/Folder/AssetName' or '/Game/Folder/AssetName.AssetName'. Examples: '/Game/MyAsset', '/Game/Blueprints/BP_Player', '/Engine/EditorMaterials/GridMaterial'. Must start with '/Game/' or '/Engine/'. Asset must exist in the project."
                },
                "maxDepth": {
                    "type": "number",
                    "description": "Maximum recursion depth to prevent infinite loops. Defaults to 10. Must be at least 1. Increase for deeper dependency trees, but be aware that very deep trees can be expensive to compute.",
                    "default": 10
                },
                "bIncludeHardDependencies": {
                    "type": "boolean",
                    "description": "Whether to include hard dependencies (direct references). Defaults to true. Hard dependencies are assets that are directly referenced by the asset.",
                    "default": True
                },
                "bIncludeSoftDependencies": {
                    "type": "boolean",
                    "description": "Whether to include soft dependencies (searchable references). Defaults to false. Soft dependencies are assets that are referenced via searchable references (e.g., string-based asset references).",
                    "default": False
                }
            },
            "required": ["assetPath"]
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "bSuccess": {"type": "boolean", "description": "Whether the operation completed successfully", "default": False},
                "assetPath": {"type": "string", "description": "The asset path that was queried"},
                "tree": {
                    "type": "array",
                    "items": {
                        "type": "object"
                    },
                    "description": "Array of dependency tree nodes, each containing assetPath, depth, and dependencies",
                    "default": []
                },
                "totalNodes": {"type": "number", "description": "Total number of nodes in the dependency tree", "default": 0},
                "maxDepthReached": {"type": "number", "description": "Maximum depth reached in the dependency tree", "default": 0},
                "error": {"type": "string", "description": "Error message if bSuccess is false"}
            },
            "required": ["bSuccess", "assetPath", "tree", "totalNodes", "maxDepthReached"]
        }
    }
    
    return tools

