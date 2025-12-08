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
*   **Tools:** Provides 7 built-in tools for asset operations:
    *   `search_blueprints` - Search for Blueprint assets by name, parent class, or comprehensive search
    *   `export_asset` - Export any UObject to various formats (defaults to T3D)
    *   `export_class_default` - Export class default objects (CDO) for inspection
    *   `import_asset` - Import files to create or update UObjects
    *   `query_asset` - Query single asset information from the asset registry
    *   `search_assets` - Search for assets in package paths with optional class filtering
    *   `get_project_config` - Retrieve project and engine configuration information
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

## Architecture

*   **Core Server:** `FUMCP_Server` - Main server class handling HTTP requests and JSON-RPC routing
*   **Tools:** `FUMCP_CommonTools` - Registers and implements common MCP tools
*   **Resources:** `FUMCP_CommonResources` - Registers and implements common MCP resources
*   **Types:** `UMCP_Types.h` - All MCP data structures defined as USTRUCTs
*   **HTTP Transport:** Uses Unreal Engine's `FHttpServerModule` for HTTP handling

## Repository Structure

*   `Source/`: Contains the C++ source code for the plugin.
    *   `UnrealMCPServer/Public/`: Public header files
    *   `UnrealMCPServer/Private/`: Implementation files
*   `Config/`: Configuration files for the plugin.
*   `Binaries/`: Compiled binaries (platform-specific).
*   `Intermediate/`: Temporary files generated during compilation.
*   `UnrealMCPServer.uplugin`: The plugin descriptor file, containing metadata about the plugin.
*   `LICENSE`: Contains the licensing information for this plugin.
*   `README.md`: This file.

## Current Status

This plugin is currently in **Beta**. Features and APIs might change.

### Implementation Status

*   ✅ Core HTTP server with JSON-RPC 2.0 support
*   ✅ MCP initialization handshake
*   ✅ 7 tools implemented
*   ✅ Resource system with URI templates
*   ✅ Prompt framework (ready for use)
*   ⏳ SSE/Streaming support (deferred)
*   ⏳ TLS/HTTPS support (deferred)
*   ⏳ Session management (deferred)

## Documentation

For detailed technical documentation, see:
*   `.cursor/notes/MCP_HTTP_Implementation_TechSpec.md` - Technical specification and implementation details

## Contributing

Details on contributing, reporting issues, or feature requests will be provided as the project matures. For now, please refer to the project's issue tracker if available.

## License

This project is licensed under the terms specified in the `LICENSE` file.
