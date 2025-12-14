# Unreal MCP Server Plugin

**Version:** 0.1.0 (Beta)
**Created By:** Prophecy Games, Inc

## Overview

The Unreal MCP Server is an Unreal Engine plugin that implements a Model Context Protocol (MCP) server. Its primary goal is to facilitate interaction between AI agents and the Unreal Engine environment by exposing engine functionalities through a standardized protocol.

This plugin allows external applications, particularly AI agents, to query and manipulate aspects of an Unreal Engine project in real-time using the MCP standard.

## Features

*   **HTTP Server:** The plugin runs an HTTP server on port **30069** (configurable).
*   **MCP Protocol:** Implements the Model Context Protocol (MCP) using JSON-RPC 2.0 messages.
*   **Protocol Version:** Supports MCP protocol version "2024-11-05".
*   **Tools:** Provides 14 built-in tools for asset operations and engine control:
    *   **Project & Configuration Tools:**
        *   `get_project_config` - Retrieve project and engine configuration information including engine version, directory paths (Engine, Project, Content, Log, Saved, Config, Plugins), and other essential project metadata. Use this tool first to understand the project structure before performing asset operations.
        *   `get_log_file_path` - Returns the absolute path of the Unreal Engine log file. Use this to locate log files for debugging. Log files are plain text and can be read with standard file reading tools.
    *   **Editor Control Tools:**
        *   `execute_console_command` - Execute an Unreal Engine console command and return its output. Common commands: `stat fps` (performance stats), `showdebug ai` (AI debugging), `r.SetRes 1920x1080` (set resolution), `open /Game/Maps/MainLevel` (load level), `stat unit` (frame timing). Note: Some commands modify editor state. Returns command output as a string.
        *   `request_editor_compile` - Requests an editor compilation, waits for completion, and returns whether it succeeded or failed along with any build log generated. Use this after modifying C++ source files to recompile code changes without restarting the editor. Only works if the project has C++ code and live coding is enabled.
    *   **Asset Tools:**
        *   `query_asset` - Query a single asset to check if it exists and get its basic information from the asset registry. Use this before export_asset or import_asset to verify an asset exists. Faster than export_asset for simple existence checks. Returns asset path, name, class, package path, and optionally tags.
        *   `search_assets` - Search for assets by package paths or package names, optionally filtered by class. More flexible than search_blueprints as it works with all asset types. Use packagePaths to search directories, packageNames for exact package matches, and classPaths to filter by asset type. **WARNING:** Searching `/Game/` directory without class filters is extremely expensive and not allowed. Always provide at least one class filter when searching large directories.
        *   `search_blueprints` - Search for Blueprint assets based on various criteria including name patterns, parent classes, and package paths. Use `name` searchType to find Blueprints by name pattern (e.g., `BP_Player*`), `parent_class` to find Blueprints that inherit from a class (e.g., `Actor`, `Pawn`, `Character`), or `all` for comprehensive search.
        *   `export_asset` - Export a single UObject to a specified format (defaults to T3D). Exportable asset types include: StaticMesh, Texture2D, Material, Sound, Animation, and most UObject-derived classes. T3D format provides human-readable text representation. **IMPORTANT:** This tool will fail if used with Blueprint assets. Blueprints must be exported using batch_export_assets instead.
        *   `batch_export_assets` - Export multiple assets to files in a specified folder. Required for Blueprint assets, as export_asset will fail for Blueprints due to response size limitations. Use this for Blueprints or when exporting multiple assets. Files are saved to disk at the specified output folder path. Format defaults to T3D. Each asset is exported to a separate file named after the asset. **For Blueprint graph inspection**: Use `format="md"` (markdown) when exporting Blueprint assets. The markdown export provides complete Blueprint graph information including nodes, variables, functions, and events. After export, agents should read the markdown file using standard file system tools, then parse and optionally flatten the markdown to understand the graph structure. The MCP cannot perform the simplification/flattening step - this must be done by the agent.
        *   `export_class_default` - Export the class default object (CDO) for a given class path. This allows determining default values for a class, since exporting instances of objects do not print values that are identical to the default value. Use this to understand default property values for Unreal classes. Useful for comparing instance values against defaults.
        *   `import_asset` - Import a file to create or update a UObject. The file type is automatically detected based on available factories. Supported binary formats: `.fbx`, `.obj` (meshes), `.png`, `.jpg`, `.tga` (textures), `.wav`, `.mp3` (sounds). T3D files can be used to import from T3D format or to configure imported objects. If asset exists at packagePath, it will be updated. Otherwise, a new asset is created.
        *   `get_asset_dependencies` - Get all assets that a specified asset depends on. Returns an array of asset paths that the specified asset depends on. Use this to understand what assets an asset requires, which is useful for impact analysis, refactoring safety, and understanding asset relationships. Very useful when doing asset searches and queries with existing tools. Supports both hard dependencies (direct references) and soft dependencies (searchable references).
        *   `get_asset_references` - Get all assets that reference a specified asset. Returns an array of asset paths that reference the specified asset. Use this to understand what assets depend on this asset, which is critical for impact analysis, refactoring safety, and unused asset detection. Very useful when doing asset searches and queries with existing tools. Supports both hard references (direct references) and soft references (searchable references).
        *   `get_asset_dependency_tree` - Get the complete dependency tree for a specified asset. Returns a recursive tree structure showing all dependencies and their dependencies. Use this for complete dependency mapping and recursive analysis. The tree includes depth information for each node. Very useful when doing asset searches and queries with existing tools. Supports both hard dependencies (direct references) and soft dependencies (searchable references). Use maxDepth to limit recursion depth and prevent infinite loops.
*   **Resources:** URI template-based resource system for accessing Unreal Engine assets:
    *   Blueprint T3D exporter via `unreal+t3d://{filepath}` URI scheme
*   **Prompts:** Framework for templated prompt interactions (ready for use, no prompts currently registered)
*   **JSON Schema:** Automatic JSON Schema generation from C++ USTRUCT definitions
*   **Editor Integration:** The plugin module runs in the Editor (`"Type": "Editor"`).

## Getting Started

### Prerequisites

*   Unreal Engine 5.0 or later (compatibility should be verified with your specific engine version)
*   A C++ toolchain compatible with your Unreal Engine version for compiling the plugin if you are building from source

### Installation

1.  **Clone or Download:**
    *   Place the `UnrealMCPServer` plugin folder into your Unreal Engine project's `Plugins` directory. For example: `YourProject/Plugins/UnrealMCPServer`.
    *   If your project doesn't have a `Plugins` folder, create one at the root of your project directory.

2.  **Enable the Plugin:**
    *   Open your Unreal Engine project.
    *   Go to `Edit -> Plugins`.
    *   Search for "Unreal MCP Server" (it should appear under the "MCPServer" category or "Project -> Other").
    *   Ensure the "Enabled" checkbox is ticked.
    *   You may need to restart the Unreal Editor.

3.  **Compile (if necessary):**
    *   If you added the plugin to a C++ project, Unreal Engine might prompt you to recompile the project. Allow it to do so.
    *   If you are using a Blueprint-only project, you might need to convert it to a C++ project first (File -> New C++ Class... -> None -> Create Class, then close the editor and re-open the .sln to build).

### Usage

1.  **Server Activation:** 
    *   Once the plugin is enabled and the editor is running, the HTTP server automatically starts listening on port 30069.
    *   Check the Unreal Engine output log for confirmation: `"HTTP Server started on port 30069"`.

2.  **Client Connection:**
    *   Develop a client application using any HTTP client library (e.g., Python `requests`, `httpx`, or a companion MCP client library).
    *   Connect the client to the Unreal Engine instance at `http://localhost:30069/mcp`.

3.  **MCP Protocol Usage:**
    *   The server implements the MCP standard. Clients should follow the MCP initialization handshake:
        1.  Send `initialize` request with protocol version
        2.  Receive server capabilities
        3.  Send `notifications/initialized` notification
        4.  Begin using tools, resources, and prompts
    
    *   Example `initialize` request:
        ```json
        {
          "jsonrpc": "2.0",
          "id": 1,
          "method": "initialize",
          "params": {
            "protocolVersion": "2024-11-05"
          }
        }
        ```
    
    *   Example `tools/call` request:
        ```json
        {
          "jsonrpc": "2.0",
          "id": 2,
          "method": "tools/call",
          "params": {
            "name": "query_asset",
            "arguments": {
              "assetPath": "/Game/MyAsset"
            }
          }
        }
        ```

4.  **Receiving Responses:** The server responds with JSON-RPC 2.0 formatted responses.

5.  **Tool Usage Tips:**
    *   **Start with `get_project_config`** to understand the project structure and get directory paths
    *   **Use `query_asset`** before `export_asset` or `import_asset` to verify assets exist
    *   **For Blueprints**, always use `batch_export_assets` instead of `export_asset` (which will fail)
    *   **For Blueprint graph inspection**: Use `batch_export_assets` with `format="md"` (markdown) to export Blueprint assets. The markdown export provides complete graph information. After export, read the markdown file using standard file system tools, then parse and optionally flatten it to understand the graph structure. The MCP cannot perform simplification - agents must handle parsing/flattening.
    *   **When searching assets**, always provide class filters when searching large directories like `/Game/` to avoid expensive operations
    *   **For console commands**, check the Unreal Engine output log for command output, as some commands only produce visual output
    *   **For file paths**, use absolute paths or paths relative to the project directory

5.  **T3D Format Specification:**
    *   When working with T3D (Unreal Text File) format files for asset export, import, or analysis, it is recommended that your project rules reference the T3D format specification document.
    *   This helps AI agents and developers understand the T3D file structure, syntax, and parsing guidelines.
    *   See the [T3D Format Specification Reference](#t3d-format-specification-reference) section below for details on adding this reference to your project rules.

## Architecture

*   **Core Server:** `FUMCP_Server` - Main server class handling HTTP requests and JSON-RPC routing
*   **Tools:** 
    *   `FUMCP_CommonTools` - Registers and implements general-purpose MCP tools (project config, console commands, editor compilation)
    *   `FUMCP_AssetTools` - Registers and implements asset-related MCP tools (search, export, import, query, dependency analysis)
    *   `FUMCP_BlueprintTools` - Registers and implements Blueprint-specific MCP tools (search, markdown export)
*   **Resources:** `FUMCP_CommonResources` - Registers and implements common MCP resources
*   **Types:** 
    *   `UMCP_Types.h` - Core MCP protocol data structures (JSON-RPC, Initialize, ToolDefinition, etc.)
    *   `UMCP_AssetTools.h` - Asset tool parameter and result types
    *   `UMCP_CommonTools.h` - Common tool parameter and result types
    *   `UMCP_BlueprintTools.h` - Blueprint tool parameter and result types
*   **HTTP Transport:** Uses Unreal Engine's `FHttpServerModule` for HTTP handling

## UnrealMCPProxy

This project includes **UnrealMCPProxy**, a Python-based MCP proxy server that provides:

*   **Always-Alive Availability**: The proxy remains available even when the Unreal Editor restarts, providing continuous MCP service
*   **Robust MCP Presentation**: Uses FastMCP library for well-known, standardized MCP protocol implementation
*   **Offline Tool Lists**: Can provide tool definitions even when the backend is offline
*   **Enhanced Error Handling**: Provides graceful error handling and retry logic for read-only operations

### Proxy Maintenance Requirements

**CRITICAL: The UnrealMCPProxy MUST be kept synchronized with the backend.**

When you modify tools, descriptions, parameters, or schemas in the UnrealMCPServer plugin, you **MUST** also update the corresponding tool definitions in the UnrealMCPProxy. This ensures:

*   The proxy can provide accurate tool lists when the backend is offline
*   Tool calls are properly forwarded with correct parameter schemas
*   Clients receive consistent tool definitions regardless of backend availability

### How to Update the Proxy

1. **Identify the affected tool domain** (Common, Asset, or Blueprint)
2. **Update the corresponding file** in `UnrealMCPProxy/src/unreal_mcp_proxy/`:
   - Common Tools → `tool_definitions_common.py`
   - Asset Tools → `tool_definitions_asset.py`
   - Blueprint Tools → `tool_definitions_blueprint.py`
3. **Run the test suite** to verify schema compatibility:
   ```bash
   cd UnrealMCPProxy
   uv --directory . run test_proxy.py
   ```
4. **Verify offline tool listing** works correctly

See `.cursorrules` and `docs/SCHEMA_SYNCHRONIZATION_GUIDE.md` for detailed requirements and best practices.

**Important**: The proxy's tool definitions must be **compatible** with the backend schemas (all required fields present, compatible types), but can have improved descriptions, simplified types, and better defaults.

## Repository Structure

*   `Source/`: Contains the C++ source code for the plugin.
    *   `UnrealMCPServer/Public/`: Public header files
        *   `UMCP_Server.h` - Main server class
        *   `UMCP_CommonTools.h` - General-purpose tools and their parameter/result types
        *   `UMCP_AssetTools.h` - Asset-related tools and their parameter/result types
        *   `UMCP_BlueprintTools.h` - Blueprint-specific tools and their parameter/result types
        *   `UMCP_CommonResources.h` - Resource definitions
        *   `UMCP_Types.h` - Core MCP protocol data structures (JSON-RPC, Initialize, ToolDefinition, etc.)
    *   `UnrealMCPServer/Private/`: Implementation files
        *   `UMCP_Server.cpp` - Server implementation
        *   `UMCP_CommonTools.cpp` - Common tools implementation
        *   `UMCP_AssetTools.cpp` - Asset tools implementation
        *   `UMCP_CommonResources.cpp` - Resources implementation
        *   `UMCP_Types.cpp` - Type utilities
*   `Config/`: Configuration files for the plugin.
*   `Binaries/`: Compiled binaries (platform-specific).
*   `Intermediate/`: Temporary files generated during compilation.
*   `UnrealMCPServer.uplugin`: The plugin descriptor file, containing metadata about the plugin.
*   `LICENSE`: Contains the licensing information for this plugin.
*   `README.md`: This file.

## T3D Format Specification Reference

When working with T3D (Unreal Text File) format files for asset export, import, or analysis, refer to the **T3D File Format Specification** document.

**Important:** Users should make sure their project references the T3D format specification in their project rules. This ensures that AI agents and developers have access to complete T3D format documentation when working with exported assets.

### Adding T3D Format Reference to Project Rules

Add the following section to your project's rules or documentation:

---

## T3D File Format Reference

When working with T3D (Unreal Text File) format files for asset export, import, or analysis, refer to the **T3D File Format Specification** document.

**Reference:** See `docs/T3D_FORMAT_SPECIFICATION.md` for complete T3D format documentation, including:

- File structure and syntax

- Block types (Map, Level, Actor, Component, Brush)

- Property formats and value types

- Object reference syntax

- Blueprint export format

- Parsing guidelines for AI agents

- Validation checklist

- Common actor classes and examples

**Key Use Cases:**

- Exporting Unreal Engine assets to T3D format via MCP tools

- Interpreting T3D files exported from Blueprints or other assets

- Understanding object references and dependencies in exported assets

- Validating T3D file structure and content

---

This reference helps ensure that when AI agents or developers interact with T3D files through the MCP server tools (such as `export_asset`, `import_asset`, or `export_class_default`), they have access to the complete format specification for proper interpretation and manipulation of T3D data.

## Documentation

For detailed technical documentation, see:
*   `docs/MCP_HTTP_Implementation_TechSpec.md` - Technical specification and implementation details

## Contributing

Details on contributing, reporting issues, or feature requests will be provided as the project matures. For now, please refer to the project's issue tracker if available.

## License

This project is licensed under the terms specified in the `LICENSE` file.
