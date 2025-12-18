"""UnrealMCPProxy - Main FastMCP server implementation."""

from __future__ import annotations

import asyncio
import logging
import sys
from pathlib import Path
from typing import Optional, Dict, Any, List

# Path manipulation for script execution
# This allows the module to be run directly as a script (e.g., `python -m src.unreal_mcp_proxy.server`)
# while still supporting absolute imports. This is a standard pattern for Python packages that need
# to be both importable modules and executable scripts.
# 
# When run as a script, __package__ is None, so we add the src directory to sys.path.
# When imported as a module, __package__ is set and imports work normally.
try:
    package = __package__
except NameError:
    package = None

# Determine if running as script or module
is_script = not package

if is_script:
    # Running as script - add src directory to path for absolute imports
    current_file = Path(__file__).resolve()
    src_dir = current_file.parent.parent  # src/unreal_mcp_proxy/server.py -> src/
    if str(src_dir) not in sys.path:
        sys.path.insert(0, str(src_dir))

from fastmcp import FastMCP

# Import from new modules - use absolute imports when running as script
if is_script:
    # Absolute imports when running as script
    from unreal_mcp_proxy.config import ServerSettings, MCPTransport, setup_logging
    from unreal_mcp_proxy.errors import create_error_response
    from unreal_mcp_proxy.compatibility import check_tool_compatibility
    from unreal_mcp_proxy.tools import call_tool, handle_tool_result
    from unreal_mcp_proxy.client.unreal_mcp import UnrealMCPClient, ConnectionState, UnrealMCPSettings
    from unreal_mcp_proxy.tool_definitions import get_tool_definitions
    from unreal_mcp_proxy.tool_decorators import read_only, write_operation
    from unreal_mcp_proxy.constants import DEFAULT_EXPORT_FORMAT, DEFAULT_COMPILATION_TIMEOUT
    from unreal_mcp_proxy.initialization import initialize_proxy
    from unreal_mcp_proxy.resource_definitions import get_cached_resource_definitions
    from unreal_mcp_proxy.prompt_definitions import get_cached_prompt_definitions, generate_prompt_messages
else:
    # Relative imports when running as module
    from .config import ServerSettings, MCPTransport, setup_logging
    from .errors import create_error_response
    from .compatibility import check_tool_compatibility
    from .tools import call_tool, handle_tool_result
    from .client.unreal_mcp import UnrealMCPClient, ConnectionState, UnrealMCPSettings
    from .tool_definitions import get_tool_definitions
    from .tool_decorators import read_only, write_operation
    from .constants import DEFAULT_EXPORT_FORMAT, DEFAULT_COMPILATION_TIMEOUT
    from .initialization import initialize_proxy
    from .resource_definitions import get_cached_resource_definitions
    from .prompt_definitions import get_cached_prompt_definitions, generate_prompt_messages

# Initialize settings and logging
settings = ServerSettings()
setup_logging(settings)
logger = logging.getLogger(__name__)

# Initialize FastMCP server
mcp = FastMCP("unreal-mcp-proxy")

# Initialize backend client with health check interval from settings
client_settings = UnrealMCPSettings()
client_settings.health_check_interval = settings.health_check_interval
unreal_client = UnrealMCPClient(settings=client_settings)

# Proxy tool definitions loaded from tool_definitions.py
# These are the proxy's static tool definitions (not backend tools).
# Used for: compatibility checking, read-only detection, and validation
# Backend tools are fetched dynamically via unreal_client.get_tools_list()
_cached_proxy_tool_definitions: Optional[Dict[str, Dict[str, Any]]] = None


# Compatibility checking helper
async def check_compatibility_when_online():
    """Check compatibility when backend comes online."""
    # Wait a bit for health check to establish connection
    await asyncio.sleep(1)
    if unreal_client.state == ConnectionState.ONLINE:
        await check_tool_compatibility(unreal_client, _cached_proxy_tool_definitions)


# Wrapper function for tool calls that provides the necessary context
async def _call_tool_wrapper(tool_name: str, arguments: Dict[str, Any]) -> Dict[str, Any]:
    """Wrapper for call_tool that provides context from module-level variables."""
    return await call_tool(
        tool_name,
        arguments,
        unreal_client,
        settings,
        _cached_proxy_tool_definitions,
        check_compatibility_when_online
    )


# Wrapper function for handling tool results
async def _handle_tool_result_wrapper(tool_name: str, result: Dict[str, Any]) -> Dict[str, Any]:
    """Wrapper for handle_tool_result."""
    return await handle_tool_result(tool_name, result)


async def start_server():
    """Start the proxy server (for programmatic use)."""
    logger.info(f"UnrealMCPProxy server ready (transport={settings.transport.value})")
    return mcp


# Static tool functions - all 15 tools hardcoded
# FastMCP auto-generates schemas from function signatures.
# These must be kept in sync with backend tool definitions.

@mcp.tool(
    annotations={
        "title": "Get Project Configuration",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": True
    }
)
@read_only
async def get_project_config() -> Dict[str, Any]:
    """Retrieve project and engine configuration information including engine version, directory paths (Engine, Project, Content, Log, Saved, Config, Plugins), and other essential project metadata. Use this tool first to understand the project structure before performing asset operations. Returns absolute paths that can be used in other tool calls."""
    result = await _call_tool_wrapper("get_project_config", {})
    return await _handle_tool_result_wrapper("get_project_config", result)

@mcp.tool(
    annotations={
        "title": "Execute Console Command",
        "readOnlyHint": False,
        "destructiveHint": True,
        "idempotentHint": False
    }
)
@write_operation
async def execute_console_command(command: str) -> Dict[str, Any]:
    """Execute an Unreal Engine console command and return its output. This allows running any console command available in the Unreal Engine editor. Common commands: 'stat fps' (performance stats), 'showdebug ai' (AI debugging), 'r.SetRes 1920x1080' (set resolution), 'open /Game/Maps/MainLevel' (load level), 'stat unit' (frame timing). Note: Some commands modify editor state. Returns command output as a string. Some commands may return empty strings if they only produce visual output in the editor."""
    result = await _call_tool_wrapper("execute_console_command", {"command": command})
    return await _handle_tool_result_wrapper("execute_console_command", result)

@mcp.tool(
    annotations={
        "title": "Export Asset",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": True
    }
)
@read_only
async def export_asset(objectPath: str, format: str = DEFAULT_EXPORT_FORMAT) -> Dict[str, Any]:
    """Export a single UObject to a specified format (defaults to T3D). Exportable asset types include: StaticMesh, Texture2D, Material, Sound, Animation, and most UObject-derived classes. Returns the exported content as a string. T3D format provides human-readable text representation of Unreal objects."""
    result = await _call_tool_wrapper("export_asset", {"objectPath": objectPath, "format": format})
    return await _handle_tool_result_wrapper("export_asset", result)

@mcp.tool(
    annotations={
        "title": "Get Log File Path",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": True
    }
)
@read_only
async def get_log_file_path() -> Dict[str, Any]:
    """Returns the absolute path of the Unreal Engine log file. Use this to locate log files for debugging. Log files are plain text and can be read with standard file reading tools. Note: The log file path changes when the editor restarts. Call this tool when you need the current log file location."""
    result = await _call_tool_wrapper("get_log_file_path", {})
    return await _handle_tool_result_wrapper("get_log_file_path", result)

@mcp.tool(
    annotations={
        "title": "Request Editor Compilation",
        "readOnlyHint": False,
        "destructiveHint": False,
        "idempotentHint": False
    }
)
@write_operation
async def request_editor_compile(timeoutSeconds: float = DEFAULT_COMPILATION_TIMEOUT) -> Dict[str, Any]:
    """Requests an editor compilation, waits for completion, and returns whether it succeeded or failed along with any build log generated. Use this after modifying C++ source files to recompile code changes without restarting the editor. Only works if the project has C++ code and live coding is enabled in editor settings. Default timeout is 300 seconds (5 minutes). Compilation may take longer for large projects. Returns success status, build log, and extracted errors/warnings. Check the build log for compilation errors if compilation fails."""
    result = await _call_tool_wrapper("request_editor_compile", {"timeoutSeconds": timeoutSeconds})
    return await _handle_tool_result_wrapper("request_editor_compile", result)

@mcp.tool(
    annotations={
        "title": "Query Asset",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": True
    }
)
@read_only
async def query_asset(assetPath: str, bIncludeTags: bool = False) -> Dict[str, Any]:
    """Query a single asset to check if it exists and get its basic information from the asset registry. Use this before export_asset or import_asset to verify an asset exists. Faster than export_asset for simple existence checks. Returns asset path, name, class, package path, and optionally tags. Returns error if asset doesn't exist."""
    result = await _call_tool_wrapper("query_asset", {"assetPath": assetPath, "bIncludeTags": bIncludeTags})
    return await _handle_tool_result_wrapper("query_asset", result)

@mcp.tool(
    annotations={
        "title": "Search Blueprints",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": True
    }
)
@read_only
async def search_blueprints(searchType: str, searchTerm: str, packagePath: Optional[str] = None, bRecursive: bool = True, maxResults: int = 0, offset: int = 0) -> Dict[str, Any]:
    """Search for Blueprint assets based on various criteria including name patterns, parent classes, and package paths. Returns array of Blueprint asset information including paths, names, parent classes, and match details. Use 'name' searchType to find Blueprints by name pattern (e.g., 'BP_Player*'), 'parent_class' to find Blueprints that inherit from a class (e.g., 'Actor', 'Pawn', 'Character'), or 'all' for comprehensive search across all criteria. Use maxResults and offset for paging through large result sets."""
    kwargs = {"searchType": searchType, "searchTerm": searchTerm, "bRecursive": bRecursive, "maxResults": maxResults, "offset": offset}
    if packagePath is not None:
        kwargs["packagePath"] = packagePath
    result = await _call_tool_wrapper("search_blueprints", kwargs)
    return await _handle_tool_result_wrapper("search_blueprints", result)

@mcp.tool(
    annotations={
        "title": "Batch Export Assets",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": False
    }
)
@read_only
async def batch_export_assets(objectPaths: List[str], outputFolder: str, format: str = DEFAULT_EXPORT_FORMAT) -> Dict[str, Any]:
    """Export multiple assets to files in a specified folder. Returns a list of the exported file paths. Required for Blueprint assets, as export_asset will fail for Blueprints due to response size limitations. Use this when exporting multiple assets of any type. Files are saved to disk at the specified output folder path. Format defaults to T3D. Each asset is exported to a separate file named after the asset. Returns array of successfully exported file paths. Failed exports are not included in the return value. NOTE: For Blueprint graph inspection, use export_blueprint_markdown instead, which is specifically designed for that purpose and provides clearer workflow guidance."""
    # objectPaths defaults to empty array in backend, but we require it to be provided
    result = await _call_tool_wrapper("batch_export_assets", {"objectPaths": objectPaths, "outputFolder": outputFolder, "format": format})
    return await _handle_tool_result_wrapper("batch_export_assets", result)

@mcp.tool(
    annotations={
        "title": "Export Class Default",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": True
    }
)
@read_only
async def export_class_default(classPath: str, format: str = DEFAULT_EXPORT_FORMAT) -> Dict[str, Any]:
    """Export the class default object (CDO) for a given class path. This allows determining default values for a class, since exporting instances of objects do not print values that are identical to the default value. Use this to understand default property values for Unreal classes. Useful for comparing instance values against defaults. Returns T3D format by default, showing all default property values for the class."""
    result = await _call_tool_wrapper("export_class_default", {"classPath": classPath, "format": format})
    return await _handle_tool_result_wrapper("export_class_default", result)

@mcp.tool(
    annotations={
        "title": "Import Asset",
        "readOnlyHint": False,
        "destructiveHint": True,
        "idempotentHint": False
    }
)
@write_operation
async def import_asset(packagePath: str, classPath: str, filePath: Optional[str] = None, t3dFilePath: Optional[str] = None) -> Dict[str, Any]:
    """Import a file to create or update a UObject. The file type is automatically detected based on available factories. Import binary files (textures, meshes, sounds) or T3D files to create/update Unreal assets. Supported binary formats: .fbx, .obj (meshes), .png, .jpg, .tga (textures), .wav, .mp3 (sounds). T3D files can be used to import from T3D format or to configure imported objects. If asset exists at packagePath, it will be updated. Otherwise, a new asset is created. At least one of filePath (binary) or t3dFilePath (T3D) must be provided."""
    kwargs = {"packagePath": packagePath, "classPath": classPath}
    if filePath is not None:
        kwargs["filePath"] = filePath
    if t3dFilePath is not None:
        kwargs["t3dFilePath"] = t3dFilePath
    result = await _call_tool_wrapper("import_asset", kwargs)
    return await _handle_tool_result_wrapper("import_asset", result)

@mcp.tool(
    annotations={
        "title": "Search Assets",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": True
    }
)
@read_only
async def search_assets(packagePaths: List[str], packageNames: List[str], classPaths: List[str] = None, bRecursive: bool = True, bIncludeTags: bool = False, maxResults: int = 0, offset: int = 0) -> Dict[str, Any]:
    """Search for assets by package paths or package names, optionally filtered by class. Returns an array of asset information from the asset registry. More flexible than search_blueprints as it works with all asset types. REQUIRED: At least one of 'packagePaths' or 'packageNames' must be a non-empty array. Use packagePaths to search directories (e.g., '/Game/Blueprints' searches all assets in that folder), packageNames for exact or partial package matches (supports wildcards * and ?, or substring matching), and classPaths to filter by asset type (e.g., textures only). Returns array of asset information. Use bIncludeTags=true to get additional metadata tags. Use maxResults and offset for paging through large result sets. For large searches, use maxResults to limit results and offset for paging."""
    # Apply default for optional classPaths
    if classPaths is None:
        classPaths = []
    # Backend validates that at least one of packagePaths or packageNames is non-empty
    result = await _call_tool_wrapper("search_assets", {"packagePaths": packagePaths, "packageNames": packageNames, "classPaths": classPaths, "bRecursive": bRecursive, "bIncludeTags": bIncludeTags, "maxResults": maxResults, "offset": offset})
    return await _handle_tool_result_wrapper("search_assets", result)

@mcp.tool(
    annotations={
        "title": "Get Asset Dependencies",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": True
    }
)
@read_only
async def get_asset_dependencies(assetPath: str, bIncludeHardDependencies: bool = True, bIncludeSoftDependencies: bool = False) -> Dict[str, Any]:
    """Get all assets that a specified asset depends on. Returns an array of asset paths that the specified asset depends on. Use this to understand what assets an asset requires, which is useful for impact analysis, refactoring safety, and understanding asset relationships. Very useful when doing asset searches and queries with existing tools. Supports both hard dependencies (direct references) and soft dependencies (searchable references)."""
    result = await _call_tool_wrapper("get_asset_dependencies", {"assetPath": assetPath, "bIncludeHardDependencies": bIncludeHardDependencies, "bIncludeSoftDependencies": bIncludeSoftDependencies})
    return await _handle_tool_result_wrapper("get_asset_dependencies", result)

@mcp.tool(
    annotations={
        "title": "Get Asset References",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": True
    }
)
@read_only
async def get_asset_references(assetPath: str, bIncludeHardReferences: bool = True, bIncludeSoftReferences: bool = False) -> Dict[str, Any]:
    """Get all assets that reference a specified asset. Returns an array of asset paths that reference the specified asset. Use this to understand what assets depend on this asset, which is critical for impact analysis, refactoring safety, and unused asset detection. Very useful when doing asset searches and queries with existing tools. Supports both hard references (direct references) and soft references (searchable references)."""
    result = await _call_tool_wrapper("get_asset_references", {"assetPath": assetPath, "bIncludeHardReferences": bIncludeHardReferences, "bIncludeSoftReferences": bIncludeSoftReferences})
    return await _handle_tool_result_wrapper("get_asset_references", result)

@mcp.tool(
    annotations={
        "title": "Get Asset Dependency Tree",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": True
    }
)
@read_only
async def get_asset_dependency_tree(assetPath: str, maxDepth: int = 10, bIncludeHardDependencies: bool = True, bIncludeSoftDependencies: bool = False) -> Dict[str, Any]:
    """Get the complete dependency tree for a specified asset. Returns a recursive tree structure showing all dependencies and their dependencies. Use this for complete dependency mapping and recursive analysis. The tree includes depth information for each node. Very useful when doing asset searches and queries with existing tools. Supports both hard dependencies (direct references) and soft dependencies (searchable references). Use maxDepth to limit recursion depth and prevent infinite loops."""
    result = await _call_tool_wrapper("get_asset_dependency_tree", {"assetPath": assetPath, "maxDepth": maxDepth, "bIncludeHardDependencies": bIncludeHardDependencies, "bIncludeSoftDependencies": bIncludeSoftDependencies})
    return await _handle_tool_result_wrapper("get_asset_dependency_tree", result)

@mcp.tool(
    annotations={
        "title": "Export Blueprint Markdown",
        "readOnlyHint": True,
        "destructiveHint": False,
        "idempotentHint": False
    }
)
@read_only
async def export_blueprint_markdown(blueprintPaths: List[str], outputFolder: str) -> Dict[str, Any]:
    """Export Blueprint asset(s) to markdown format for graph inspection. This is the recommended method for inspecting Blueprint graph structure, as Blueprint exports are too large to return directly in responses. The markdown export provides complete Blueprint graph information including nodes, variables, functions, and events. Files are saved to disk at the specified output folder path. Each Blueprint is exported to a separate markdown file named after the asset. Returns array of successfully exported file paths."""
    # blueprintPaths defaults to empty array in backend, but we require it to be provided
    result = await _call_tool_wrapper("export_blueprint_markdown", {"blueprintPaths": blueprintPaths, "outputFolder": outputFolder})
    return await _handle_tool_result_wrapper("export_blueprint_markdown", result)

# Resource handlers - forward to backend with offline caching
# FastMCP resources use URI templates, so we register handlers for the backend's resource templates
# These handlers forward to backend when online, and use cached definitions when offline

@mcp.resource("unreal+t3d://{filepath}")
async def read_t3d_resource(filepath: str) -> str:
    """Read T3D Blueprint resource from backend.
    
    Args:
        filepath: The Blueprint file path (e.g., '/Game/MyBlueprint')
    
    Returns:
        T3D content as string
    """
    uri = f"unreal+t3d://{filepath}"
    if unreal_client.state == ConnectionState.OFFLINE:
        logger.warning(f"Backend unavailable for resource: {uri}")
        return "Unreal MCP server is not available. Please ensure Unreal Editor is running and the UnrealMCPServer plugin is enabled."
    
    try:
        response = await unreal_client.read_resource(uri)
        if "error" in response:
            error = response["error"]
            logger.error(f"Backend returned error for resource {uri}: {error}")
            return f"Error: {error.get('message', 'Unknown error')}"
        
        result = response.get("result", {})
        contents = result.get("contents", [])
        if contents and len(contents) > 0:
            return contents[0].get("text", "")
        return "Resource content not available"
    except Exception as e:
        logger.error(f"Error reading resource {uri}: {str(e)}", exc_info=True)
        return f"Error reading resource: {str(e)}"

# Conditionally register markdown resource handler based on enable_markdown_export setting
if settings.enable_markdown_export:
    @mcp.resource("unreal+md://{filepath}")
    async def read_markdown_resource(filepath: str) -> str:
        """Read Markdown Blueprint summary resource from backend.
        
        Args:
            filepath: The Blueprint file path (e.g., '/Game/MyBlueprint')
        
        Returns:
            Markdown content as string
        """
        uri = f"unreal+md://{filepath}"
        if unreal_client.state == ConnectionState.OFFLINE:
            logger.warning(f"Backend unavailable for resource: {uri}")
            return "Unreal MCP server is not available. Please ensure Unreal Editor is running and the UnrealMCPServer plugin is enabled."
        
        try:
            response = await unreal_client.read_resource(uri)
            if "error" in response:
                error = response["error"]
                logger.error(f"Backend returned error for resource {uri}: {error}")
                return f"Error: {error.get('message', 'Unknown error')}"
            
            result = response.get("result", {})
            contents = result.get("contents", [])
            if contents and len(contents) > 0:
                return contents[0].get("text", "")
            return "Resource content not available"
        except Exception as e:
            logger.error(f"Error reading resource {uri}: {str(e)}", exc_info=True)
            return f"Error reading resource: {str(e)}"

# Resource and Prompt list handlers - forward to backend with offline fallback
# FastMCP automatically handles resources/list and prompts/list from registered resources/prompts,
# but we need to intercept these to provide forwarding with offline caching

async def _get_resources_list_forward(cursor: Optional[str] = None) -> Dict[str, Any]:
    """Get resources list, forwarding to backend when online, using cache when offline."""
    if unreal_client.state == ConnectionState.ONLINE:
        try:
            response = await unreal_client.get_resources_list(cursor)
            if "error" not in response:
                return response.get("result", {"resources": [], "nextCursor": ""})
        except Exception as e:
            logger.warning(f"Error getting resources from backend, using cache: {str(e)}")
    
    # Use cached definitions when offline or on error
    cached = get_cached_resource_definitions()
    return {
        "resources": cached.get("resources", []),
        "nextCursor": ""
    }

async def _get_resource_templates_list_forward(cursor: Optional[str] = None) -> Dict[str, Any]:
    """Get resource templates list, forwarding to backend when online, using cache when offline."""
    if unreal_client.state == ConnectionState.ONLINE:
        try:
            response = await unreal_client.get_resource_templates_list(cursor)
            if "error" not in response:
                return response.get("result", {"resourceTemplates": [], "nextCursor": ""})
        except Exception as e:
            logger.warning(f"Error getting resource templates from backend, using cache: {str(e)}")
    
    # Use cached definitions when offline or on error
    cached = get_cached_resource_definitions()
    return {
        "resourceTemplates": cached.get("resourceTemplates", []),
        "nextCursor": ""
    }

# Prompt handlers - fully static (like tools)
# Prompts are just text templates, so they can be fully implemented statically
# No backend connection required - prompts work completely offline

@mcp.prompt()
def analyze_blueprint(blueprint_path: str, focus_areas: str = "all") -> List[Dict[str, Any]]:
    """Analyze a Blueprint's structure, functionality, and design patterns.
    
    Provides comprehensive analysis including variables, functions, events, graph structure,
    design patterns, dependencies, potential issues, and improvement suggestions.
    
    Args:
        blueprint_path: The path to the Blueprint asset (e.g., '/Game/Blueprints/BP_Player')
        focus_areas: Comma-separated list of areas to focus on: 'variables', 'functions', 'events', 'graph', 'design', or 'all' (default: 'all')
    
    Returns:
        List of prompt messages for LLM interaction
    """
    return generate_prompt_messages("analyze_blueprint", {
        "blueprint_path": blueprint_path,
        "focus_areas": focus_areas
    })

@mcp.prompt()
def refactor_blueprint(blueprint_path: str, refactor_goal: str) -> List[Dict[str, Any]]:
    """Generate a refactoring plan for a Blueprint.
    
    Provides current state analysis, refactoring strategy, step-by-step plan,
    breaking changes, testing plan, and migration guide.
    
    Args:
        blueprint_path: The path to the Blueprint asset
        refactor_goal: The goal of the refactoring (e.g., 'improve performance', 'add new feature', 'simplify structure')
    
    Returns:
        List of prompt messages for LLM interaction
    """
    return generate_prompt_messages("refactor_blueprint", {
        "blueprint_path": blueprint_path,
        "refactor_goal": refactor_goal
    })

@mcp.prompt()
def audit_assets(asset_paths: str, audit_type: str = "dependencies") -> List[Dict[str, Any]]:
    """Audit project assets for dependencies, references, or issues.
    
    Provides asset inventory, dependency analysis, reference analysis, unused assets,
    orphaned assets, circular dependencies, and recommendations.
    
    Args:
        asset_paths: Comma-separated list of asset paths to audit
        audit_type: Type of audit: 'dependencies', 'references', 'unused', 'orphaned', or 'all' (default: 'dependencies')
    
    Returns:
        List of prompt messages for LLM interaction
    """
    return generate_prompt_messages("audit_assets", {
        "asset_paths": asset_paths,
        "audit_type": audit_type
    })

@mcp.prompt()
def create_blueprint(blueprint_name: str, parent_class: str, description: str) -> List[Dict[str, Any]]:
    """Generate a design plan for creating a new Blueprint.
    
    Provides Blueprint structure, component requirements, initialization logic,
    core functionality, event handlers, dependencies, implementation steps, and testing checklist.
    
    Args:
        blueprint_name: Name for the new Blueprint (e.g., 'BP_PlayerController')
        parent_class: Parent class to inherit from (e.g., 'PlayerController', 'Actor', 'Pawn')
        description: Description of what the Blueprint should do
    
    Returns:
        List of prompt messages for LLM interaction
    """
    return generate_prompt_messages("create_blueprint", {
        "blueprint_name": blueprint_name,
        "parent_class": parent_class,
        "description": description
    })

@mcp.prompt()
def analyze_performance(blueprint_path: str) -> List[Dict[str, Any]]:
    """Analyze the performance characteristics of a Blueprint.
    
    Identifies performance hotspots, tick analysis, memory usage, event frequency,
    optimization opportunities, best practices, and profiling recommendations.
    
    Args:
        blueprint_path: The path to the Blueprint asset
    
    Returns:
        List of prompt messages for LLM interaction
    """
    return generate_prompt_messages("analyze_performance", {
        "blueprint_path": blueprint_path
    })

# Load proxy tool definitions for compatibility checking and read-only detection
# This happens at module load time so tool definitions are available immediately
logger.info("Loading proxy tool definitions...")
_cached_proxy_tool_definitions = get_tool_definitions(enable_markdown_export=settings.enable_markdown_export)
logger.info(f"Loaded {len(_cached_proxy_tool_definitions)} proxy tool definitions")

# Initialize proxy components (health check, compatibility checking)
# This is done lazily on first tool call if event loop is not available at module load time
async def _initialize_proxy_components():
    """Initialize proxy components when event loop is available."""
    await initialize_proxy(settings, unreal_client, _cached_proxy_tool_definitions)

if __name__ == "__main__":
    logger.info(f"Starting UnrealMCPProxy server (transport={settings.transport.value})")
    
    # Note: Proxy initialization will happen automatically on first tool call
    # when the event loop is available. This avoids event loop conflicts with FastMCP.
    # FastMCP will create and manage the event loop when mcp.run() is called.
    
    # Prepare run parameters based on transport
    run_kwargs = {"transport": settings.transport.value}
    if settings.transport != MCPTransport.stdio:
        # Only add host/port for non-stdio transports
        run_kwargs["host"] = settings.host
        run_kwargs["port"] = settings.port
        run_kwargs["stateless_http"] = True
    
    mcp.run(**run_kwargs)
