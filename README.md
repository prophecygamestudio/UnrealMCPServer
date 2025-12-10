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
*   **Tools:** Provides 8 built-in tools for asset operations and engine control:
    *   **Asset Tools:**
        *   `search_blueprints` - Search for Blueprint assets by name, parent class, or comprehensive search
        *   `export_asset` - Export any UObject to various formats (defaults to T3D)
        *   `export_class_default` - Export class default objects (CDO) for inspection
        *   `import_asset` - Import files to create or update UObjects
        *   `query_asset` - Query single asset information from the asset registry
        *   `search_assets` - Search for assets in package paths with optional class filtering
    *   **Common Tools:**
        *   `get_project_config` - Retrieve project and engine configuration information
        *   `execute_console_command` - Execute Unreal Engine console commands (e.g., `stat fps`, `showdebug ai`)
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

5.  **T3D Format Specification:**
    *   When working with T3D (Unreal Text File) format files for asset export, import, or analysis, it is recommended that your project rules reference the T3D format specification document.
    *   This helps AI agents and developers understand the T3D file structure, syntax, and parsing guidelines.
    *   See the [T3D Format Specification Reference](#t3d-format-specification-reference) section below for details on adding this reference to your project rules.

## Architecture

*   **Core Server:** `FUMCP_Server` - Main server class handling HTTP requests and JSON-RPC routing
*   **Tools:** 
    *   `FUMCP_CommonTools` - Registers and implements general-purpose MCP tools (project config, console commands)
    *   `FUMCP_AssetTools` - Registers and implements asset-related MCP tools (search, export, import, query)
*   **Resources:** `FUMCP_CommonResources` - Registers and implements common MCP resources
*   **Types:** `UMCP_Types.h` - All MCP data structures defined as USTRUCTs
*   **HTTP Transport:** Uses Unreal Engine's `FHttpServerModule` for HTTP handling

## Repository Structure

*   `Source/`: Contains the C++ source code for the plugin.
    *   `UnrealMCPServer/Public/`: Public header files
        *   `UMCP_Server.h` - Main server class
        *   `UMCP_CommonTools.h` - General-purpose tools
        *   `UMCP_AssetTools.h` - Asset-related tools
        *   `UMCP_CommonResources.h` - Resource definitions
        *   `UMCP_Types.h` - MCP data structures
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

**Reference:** See `Plugins/UnrealMCPServer/.cursor/docs/T3D_FORMAT_SPECIFICATION.md` for complete T3D format documentation, including:

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
*   `.cursor/notes/MCP_HTTP_Implementation_TechSpec.md` - Technical specification and implementation details

## Contributing

Details on contributing, reporting issues, or feature requests will be provided as the project matures. For now, please refer to the project's issue tracker if available.

## License

This project is licensed under the terms specified in the `LICENSE` file.
