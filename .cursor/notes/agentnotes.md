# Agent Notes for UnrealMCPServer Plugin

## Project Overview

This project implements an Unreal Engine plugin (`UnrealMCPServer`) that functions as a Model Context Protocol (MCP) server. The server exposes Unreal Engine capabilities (assets, editor functions, game state) to external AI models and applications.

**Current Status:** The core implementation is complete and functional. The server supports HTTP POST requests with JSON-RPC 2.0 messages. Server-Sent Events (SSE) and streaming features are deferred for future implementation.

**Initial Approach (Superseded):** The first conceptualization of this project involved an MCP server operating via a raw TCP socket (port 30069) for JSON commands.

**Current Implementation (HTTP):** The MCP server uses **HTTP POST** for communication. SSE/streaming support is planned for future implementation.

## Key Documentation

*   **Technical Specification (Current):** `MCP_HTTP_Implementation_TechSpec.md` - This is the primary design document detailing the HTTP-based MCP server architecture and current implementation status.
*   **Project Checklist:** `project_checklist.md` - Tracks the progress of tasks based on the HTTP technical specification.
*   **MCP Research:** `MCP_Server_Implementation_Research.md` - Comprehensive notes on MCP principles, JSON-RPC, HTTP/SSE transport, core features (Tools, Resources, Prompts), and security.
*   **Notebook:** `notebook.md` - General notes, findings, and ideas during development.

## Core Technical Details (HTTP Implementation)

*   **Protocol:** Model Context Protocol (MCP) using JSON-RPC 2.0 messages.
*   **Protocol Version:** "2024-11-05"
*   **Transport:** HTTP POST requests (SSE/streaming deferred).
    *   Client-to-Server: HTTP POST requests to `/mcp` endpoint (for MCP Requests/Notifications).
    *   Server-to-Client: HTTP POST response with JSON body (for MCP Responses).
    *   **SSE/Streaming:** Not yet implemented. All responses are synchronous JSON.
*   **Primary MCP Capabilities Implemented:**
    *   **Tools:** 7 tools implemented (search_blueprints, export_asset, export_class_default, import_asset, query_asset, search_assets, get_project_config).
    *   **Resources:** URI template system with Blueprint T3D exporter (`unreal+t3d://{filepath}`).
    *   **Prompts:** Framework implemented, no prompts currently registered.
*   **Unreal Engine Integration:**
    *   Implemented as an Unreal Engine Plugin (`UnrealMCPServer`).
    *   Uses `FHttpServerModule` with `IHttpRouter` for HTTP handling.
    *   Leverages Unreal's JSON parsing utilities (`FJsonObjectConverter`, `TJsonReader`/`Writer`).
    *   All MCP objects defined as `USTRUCT`s in `UMCP_Types.h`.
    *   Threading: HTTP requests processed on server threads, UE object access marshaled to game thread.
*   **Key Features:**
    *   JSON Schema generation from USTRUCTs (`UMCP_GenerateJsonSchemaFromStruct()`).
    *   URI template parsing (RFC 6570) for dynamic resource discovery.
    *   Type-safe parameter handling using USTRUCTs.
    *   Structured output support for tools with outputSchema.

## Implementation Status

### ✅ Completed
*   Core HTTP server with `/mcp` endpoint (port 30069).
*   JSON-RPC 2.0 message handling (request/response parsing and serialization).
*   MCP initialization handshake (`initialize`, `notifications/initialized`).
*   Tool system with 7 implemented tools.
*   Resource system with URI template support.
*   Prompt framework (ready for use, no prompts registered yet).
*   JSON Schema generation from USTRUCTs.
*   Type-safe parameter handling.

### ⏳ Deferred
*   SSE/Streaming support (Server-Sent Events).
*   HTTP GET endpoint for server-initiated streams.
*   `notifications/tools/list_changed`, `notifications/resources/content_changed`.
*   `resources/subscribe`, `$/progress`.
*   `shutdown`, `exit`, `$/cancelRequest` methods.
*   TLS (HTTPS) support.
*   Session management.

## Development Approach

*   Core HTTP communication and JSON-RPC handling are complete (Phase 1-3).
*   Focus on adding more tools/resources/prompts as needed.
*   Robust error handling and logging are in place.
*   Security considerations (TLS/HTTPS) are deferred for future phases.
*   SSE/streaming features are planned for future implementation.

## Python Client

A corresponding Python client library can be developed to interact with the `UnrealMCPServer`. The client should implement HTTP POST communication (SSE support can be added later when server supports it).

## Agent Reminders

*   Always refer to `MCP_HTTP_Implementation_TechSpec.md` for the current design and implementation status.
*   Ensure changes and progress are reflected in `project_checklist.md`.
*   Keep `notebook.md` updated with any new findings or important details encountered during development.
*   Adhere to user-defined rules, especially regarding code organization, testing, and documentation.
*   **Note:** The plugin name is `UnrealMCPServer`.
