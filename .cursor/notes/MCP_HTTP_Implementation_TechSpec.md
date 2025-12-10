# Technical Specification: UnrealMCPServer - MCP Server (HTTP Implementation)

**Version:** 2.0
**Date:** 2025-01
**Author:** Cascade AI

## 1. Introduction

This document outlines the technical specification for the implemented Model Context Protocol (MCP) Server within the `UnrealMCPServer` Unreal Engine plugin. This implementation adheres to the MCP standard, utilizing HTTP for communication (SSE/Streamable HTTP features are deferred for future implementation).

The server exposes Unreal Engine capabilities (assets, editor functions, game state) as MCP Tools, Resources, and Prompts, enabling external AI models and applications to interact with the Unreal Engine environment in a standardized way.

**Current Status:** The core implementation is complete and functional. The server supports HTTP POST requests with JSON-RPC 2.0 messages. Server-Sent Events (SSE) and other streaming features are deferred for future implementation.

## 2. Core Architecture in Unreal Engine

### 2.1. Server Component

The MCP Server is implemented as part of an Unreal Engine plugin (`UnrealMCPServer`). It runs within the Unreal Editor.

*   **Plugin Module:** The core server logic resides in the `UnrealMCPServerModule` (`FUnrealMCPServerModule`).
*   **Lifecycle Management:** The server starts automatically when the plugin module loads and shuts down gracefully when the module unloads or the engine exits.
*   **Core Classes:**
    *   `FUMCP_Server`: Main server class handling HTTP requests and JSON-RPC routing
    *   `FUMCP_CommonTools`: Registers and implements general-purpose MCP tools (project config, console commands)
    *   `FUMCP_AssetTools`: Registers and implements asset-related MCP tools (search, export, import, query)
    *   `FUMCP_CommonResources`: Registers and implements common MCP resources

### 2.2. HTTP Server Implementation

An HTTP server is embedded within the plugin to handle MCP communications using HTTP POST requests.

*   **HTTP Server:** Uses Unreal Engine's `FHttpServerModule` with `IHttpRouter` for routing.
*   **Port:** The server listens on port 30069 (configurable via `HttpServerPort` member variable).
*   **Endpoint:** All MCP requests are sent to `/mcp` via HTTP POST.
*   **Communication Model (Current Implementation):**
    *   Clients send JSON-RPC messages via HTTP POST to `/mcp`.
    *   The server processes the JSON-RPC message and responds with:
        *   `Content-Type: application/json`: The HTTP response body contains a single JSON-RPC Response.
        *   `HTTP 200 OK`: Standard response code for successful requests.
    *   **SSE/Streaming:** Server-Sent Events (SSE) and streaming responses are **not yet implemented**. All responses are synchronous JSON responses.
    *   **HTTP GET:** Not supported. Only HTTP POST is currently implemented.
*   **Request Processing:**
    *   Requests are received on HTTP server threads.
    *   Request handlers are marshaled to the game thread using `AsyncTask(ENamedThreads::GameThread, ...)` for UE object access.

### 2.3. Threading Model

Network I/O occurs on HTTP server threads, while UE object access is marshaled to the game thread.

*   **HTTP Listener Thread:** The HTTP server (`FHttpServerModule`) handles incoming connections on its own thread(s).
*   **Request Handler Threads:** Each incoming MCP request (via HTTP POST) is initially processed on an HTTP server thread.
*   **UE Main Thread Synchronization:** All operations requiring access to UE game objects or systems (e.g., `UWorld`, `GEditor`, asset loading) are marshaled to the game thread using `AsyncTask(ENamedThreads::GameThread, ...)`.
*   **Current Implementation:** The `HandleStreamableHTTPMCPRequest` method receives requests on HTTP threads and immediately dispatches processing to the game thread.

## 3. Protocol Mechanics in UE C++

### 3.1. JSON-RPC 2.0 Handling

*   **JSON Library:** Unreal Engine's built-in JSON utilities are used:
    *   `TJsonReader` / `TJsonWriter` for streaming.
    *   `FJsonObject` / `FJsonValue` for DOM-style manipulation.
    *   `FJsonObjectConverter` for serializing/deserializing USTRUCTs to/from JSON.
*   **Core JSON-RPC Structures:** C++ USTRUCTs define JSON-RPC messages (in `UMCP_Types.h`):
    *   `FUMCP_JsonRpcRequest`: Represents JSON-RPC requests with `jsonrpc`, `method`, `params`, and `id` fields.
    *   `FUMCP_JsonRpcResponse`: Represents JSON-RPC responses with `jsonrpc`, `result`/`error`, and `id` fields.
    *   `FUMCP_JsonRpcError`: Represents JSON-RPC error objects with `code`, `message`, and optional `data`.
    *   `FUMCP_JsonRpcId`: Encapsulates JSON-RPC ID which can be string, number, null, or absent (for notifications).
*   **Error Codes:** Standard JSON-RPC error codes are used:
    *   `-32700`: ParseError
    *   `-32600`: InvalidRequest
    *   `-32601`: MethodNotFound
    *   `-32602`: InvalidParams
    *   `-32603`: InternalError
    *   `-32000`: ServerError (base for server-specific errors)
    *   `-32002`: ResourceNotFound (MCP-specific)
*   **Property Naming Convention:** All USTRUCT properties use **camelCase** (e.g., `searchType`, `objectPath`) instead of Unreal Engine's standard PascalCase. This is intentional to align with web/JSON standards and MCP protocol expectations.
*   **JSON Schema Generation:** The codebase includes `UMCP_GenerateJsonSchemaFromStruct()` which automatically generates JSON Schema from USTRUCT definitions, enabling type-safe schema generation for tool input/output definitions.

### 3.2. MCP Connection Lifecycle

The server uses a stateless request/response model. Each HTTP POST request is handled independently.

*   **Initialization Phase (`initialize` method):**
    *   Handler: `Rpc_Initialize()` in `FUMCP_Server`.
    *   Input: `FUMCP_InitializeParams` USTRUCT (containing `protocolVersion`).
    *   Logic:
        *   Version negotiation: Server uses protocol version `"2024-11-05"` (stored in `MCP_PROTOCOL_VERSION` constant).
        *   Server capabilities are returned with default values (tools, resources, prompts capabilities).
        *   Server info includes name `"UnrealMCPServer"` and version string.
    *   Output: `FUMCP_InitializeResult` USTRUCT (containing `protocolVersion`, `serverInfo`, `capabilities`).
    *   Response is sent as JSON in the HTTP POST response body.
*   **`notifications/initialized` Notification:**
    *   Handler: `Rpc_ClientNotifyInitialized()` in `FUMCP_Server`.
    *   Currently logs the notification and returns success. No session state is maintained.
*   **Operation Phase:**
    *   Method routing: `TMap<FString, UMCP_JsonRpcHandler>` (`JsonRpcMethodHandlers`) routes incoming JSON-RPC methods to their respective handlers.
    *   Registered methods:
        *   `initialize`, `ping`, `notifications/initialized`
        *   `tools/list`, `tools/call`
        *   `resources/list`, `resources/templates/list`, `resources/read`
        *   `prompts/list`, `prompts/get`
    *   All responses are sent back in the HTTP POST response body as JSON.
*   **Shutdown Phase:**
    *   `shutdown` and `exit` methods are not yet implemented.
    *   `$/cancelRequest` is not yet implemented.

## 4. HTTP Transport Implementation

### 4.1. Server-Side Endpoint

A single base path `/mcp` is used for all MCP communication.

*   **HTTP POST `/mcp`:**
    *   Clients send all MCP Request or Notification messages to the server using HTTP POST to this endpoint.
    *   The body of the POST request contains the JSON-RPC message string (UTF-8 encoded).
    *   The server processes the JSON-RPC message:
        *   For JSON-RPC requests: The server responds with `Content-Type: application/json` containing a single JSON-RPC Response.
        *   For JSON-RPC notifications: The server processes the notification and responds with a JSON-RPC response (notifications still receive responses in the current implementation).
    *   **Current Limitation:** All responses are synchronous JSON responses. SSE streaming is not implemented.
*   **HTTP GET `/mcp`:**
    *   **Not supported.** Only HTTP POST is currently implemented.
*   **Session Management:** 
    *   **Not implemented.** The server uses a stateless request/response model. No session IDs or persistent client state is maintained.

### 4.2. Server-to-Client Message Delivery

*   MCP JSON-RPC **Responses** to client requests are delivered as a single JSON object in the HTTP POST response body (`Content-Type: application/json`).
*   Server-initiated MCP JSON-RPC **Requests** or **Notifications** (e.g., `notifications/tools/list_changed`, `$/progress`) are **not supported** as they require SSE streams, which are not yet implemented.

### 4.3. Security Considerations

*   **TLS (HTTPS):**
    *   MCP specification mandates TLS for production environments.
    *   **Decision:** For the initial implementation phases, TLS/HTTPS support will be **deferred** to simplify development. The server will operate over HTTP.
    *   Implementing HTTPS with `FHttpServerModule` might require engine modifications or be non-trivial.
    *   If using a third-party library, ensure it supports SSL/TLS (e.g., OpenSSL integration) for future implementation.
    *   **Warning:** Operating without TLS is insecure for production or sensitive data. This approach is for local development and testing only. Future work must address TLS implementation before any deployment outside of a trusted local environment.
*   **Origin Header Validation:** Servers MUST validate the `Origin` header on all incoming HTTP requests.
*   **Localhost Binding:** For local-only servers, bind to `localhost` (127.0.0.1).
*   **Authentication/Authorization:** The MCP spec largely defers this. Initially, trust incoming requests. Future enhancements could include API keys. `Mcp-Session-Id` should be cryptographically secure if used.

## 5. MCP Features Implementation (UE C++)

USTRUCTs are defined for all MCP data types (ToolDefinition, Resource, Prompt, etc.) in `UMCP_Types.h`, mirroring the MCP JSON schema.

### 5.1. Tools

*   **`tools/list`:**
    *   Handler: `Rpc_ToolsList()` in `FUMCP_Server`.
    *   Returns a list of available `FUMCP_ToolDefinition`s registered with the server.
    *   Tools are registered via `FUMCP_Server::RegisterTool()`.
    *   Currently returns all tools (no pagination/cursor support yet).
*   **`tools/call`:**
    *   Handler: `Rpc_ToolsCall()` in `FUMCP_Server`.
    *   Input: `FUMCP_CallToolParams` (`name`, `arguments` JSON object).
    *   Locates the tool by name from the registered tools map.
    *   Executes the tool via bound delegate (`FUMCP_ToolCall`).
    *   Output: `FUMCP_CallToolResult` (containing `content` array of `FUMCP_CallToolResultContent`, `isError` flag).
    *   **Structured Output Support:** If a tool defines an `outputSchema`, the server attempts to parse the tool's text output as JSON and includes it as `structuredContent` in the response.
*   **Implemented Tools:**
    *   **Asset Tools (in `FUMCP_AssetTools`):**
        1.  **`search_blueprints`**: Search for Blueprint assets by name pattern, parent class, or comprehensive search. Supports package path filtering and recursive search.
        2.  **`export_asset`**: Export any UObject to a specified format (defaults to T3D).
        3.  **`export_class_default`**: Export the class default object (CDO) for a given class path.
        4.  **`import_asset`**: Import a file to create or update a UObject. Automatically detects file type and uses appropriate factory.
        5.  **`query_asset`**: Query a single asset to check if it exists and get basic information from the asset registry.
        6.  **`search_assets`**: Search for assets in specified package paths, optionally filtered by class. Returns asset information from the asset registry.
    *   **Common Tools (in `FUMCP_CommonTools`):**
        7.  **`get_project_config`**: Retrieve project and engine configuration including engine version and directory paths.
        8.  **`execute_console_command`**: Execute Unreal Engine console commands and return their output.
*   **Tool Registration:** Tools are registered with input/output JSON schemas generated from USTRUCT definitions using `UMCP_GenerateJsonSchemaFromStruct()`.
*   **`notifications/tools/list_changed`:**
    *   **Not implemented** (requires SSE).

### 5.2. Resources

*   **`resources/list`:**
    *   Handler: `Rpc_ResourcesList()` in `FUMCP_Server`.
    *   Returns a list of `FUMCP_ResourceDefinition`s registered with the server.
    *   Resources are registered via `FUMCP_Server::RegisterResource()`.
*   **`resources/templates/list`:**
    *   Handler: `Rpc_ResourcesTemplatesList()` in `FUMCP_Server`.
    *   Returns a list of `FUMCP_ResourceTemplateDefinition`s registered with the server.
    *   Resource templates use URI templates (RFC 6570) for dynamic resource discovery.
    *   Resources are registered via `FUMCP_Server::RegisterResourceTemplate()`.
*   **`resources/read`:**
    *   Handler: `Rpc_ResourcesRead()` in `FUMCP_Server`.
    *   Input: `FUMCP_ReadResourceParams` (`uri`).
    *   First checks static resources (exact URI match).
    *   Then checks resource templates by matching the URI against registered URI templates.
    *   Returns the content of the resource via `FUMCP_ReadResourceResult` (containing `contents` array).
*   **Implemented Resources (in `FUMCP_CommonResources`):**
    1.  **Blueprint T3D Exporter** (Resource Template):
        *   URI Template: `unreal+t3d://{filepath}`
        *   Description: Exports the T3D representation of an Unreal Engine Blueprint asset.
        *   MIME Type: `application/vnd.unreal.t3d`
        *   Example URI: `unreal+t3d:///Game/MyBlueprint`
*   **URI Template System:** The codebase includes `FUMCP_UriTemplate` and `FUMCP_UriTemplateMatch` classes for parsing and matching URI templates (RFC 6570 compliant).
*   **`resources/subscribe` & `notifications/resources/content_changed`:**
    *   **Not implemented** (requires SSE).

### 5.3. Prompts

*   **`prompts/list`:**
    *   Handler: `Rpc_PromptsList()` in `FUMCP_Server`.
    *   Returns available `FUMCP_PromptDefinition`s registered with the server.
    *   Prompts are registered via `FUMCP_Server::RegisterPrompt()`.
*   **`prompts/get`:**
    *   Handler: `Rpc_PromptsGet()` in `FUMCP_Server`.
    *   Input: `FUMCP_GetPromptParams` (`name`, optional `arguments` JSON object).
    *   Locates the prompt by name and calls the bound delegate (`FUMCP_PromptGet`).
    *   Output: `FUMCP_GetPromptResult` (containing `description` and `messages` array of `FUMCP_PromptMessage`).
*   **Implemented Prompts:**
    *   **None yet.** The framework is in place, but no prompts are currently registered.
*   **Prompt Structure:** Prompts support arguments, user/assistant messages, and various content types (text, image, audio, resource).

### 5.4. Utilities

*   **`ping`:** 
    *   Handler: `Rpc_Ping()` in `FUMCP_Server`.
    *   Simple request/response to check connectivity. Returns success with no data.
*   **`$/progress`:** 
    *   **Not implemented** (requires SSE).
*   **`$/cancelRequest`:** 
    *   **Not implemented**.

## 6. Data Structures and Schema Adherence (UE C++)

*   **Source of Truth:** The official MCP JSON schema (version "2024-11-05" - server uses this protocol version).
*   **USTRUCTs:** All JSON objects defined in the MCP schema have corresponding USTRUCTs in `UMCP_Types.h`:
    *   JSON-RPC: `FUMCP_JsonRpcRequest`, `FUMCP_JsonRpcResponse`, `FUMCP_JsonRpcError`, `FUMCP_JsonRpcId`
    *   MCP Core: `FUMCP_ServerInfo`, `FUMCP_ServerCapabilities`, `FUMCP_InitializeParams`, `FUMCP_InitializeResult`
    *   Tools: `FUMCP_ToolDefinition`, `FUMCP_CallToolParams`, `FUMCP_CallToolResult`, `FUMCP_CallToolResultContent`
    *   Resources: `FUMCP_ResourceDefinition`, `FUMCP_ResourceTemplateDefinition`, `FUMCP_ReadResourceParams`, `FUMCP_ReadResourceResult`
    *   Prompts: `FUMCP_PromptDefinition`, `FUMCP_PromptDefinitionInternal`, `FUMCP_GetPromptParams`, `FUMCP_GetPromptResult`, `FUMCP_PromptMessage`, `FUMCP_PromptContent`
    *   Tool Parameters: `FUMCP_SearchBlueprintsParams`, `FUMCP_ExportAssetParams`, `FUMCP_ImportAssetParams`, etc.
*   **JSON Serialization:**
    *   `FJsonObjectConverter` is used for serialization/deserialization between JSON and USTRUCTs.
    *   Helper functions: `UMCP_ToJsonObject()`, `UMCP_ToJsonString()`, `UMCP_CreateFromJsonObject()` provide type-safe JSON conversion.
*   **JSON Schema Generation:**
    *   `UMCP_GenerateJsonSchemaFromStruct()` automatically generates JSON Schema from USTRUCT definitions.
    *   Supports property descriptions, required fields, and enum value constraints.
    *   Used extensively for tool input/output schema generation.
*   **Property Naming:**
    *   All USTRUCT properties use **camelCase** (e.g., `searchType`, `objectPath`) instead of Unreal Engine's standard PascalCase.
    *   This aligns with web/JSON standards and MCP protocol expectations.
*   **Validation:**
    *   Incoming JSON is validated by attempting to deserialize into the expected USTRUCT.
    *   Missing required fields or type mismatches result in JSON-RPC `InvalidParams` errors.
    *   Outgoing JSON generated from USTRUCTs conforms to the MCP schema.

## 7. Error Handling and Logging

*   **UE Logging:** Uses `UE_LOG` extensively with the dedicated log category `LogUnrealMCPServer`.
*   **Log Levels:** 
    *   `Error`: For critical failures (server startup, request parsing failures).
    *   `Warning`: For recoverable issues (tool execution failures, resource not found).
    *   `Log`: For normal operation information (tool execution, resource access).
    *   `Verbose`: For detailed debugging (request/response payloads, method routing).
*   **JSON-RPC Errors:** All errors are reported as valid JSON-RPC error responses using `FUMCP_JsonRpcError`.
*   **Error Codes:** Standard JSON-RPC error codes are used (see Section 3.1).
*   **Exception Handling:** C++ exceptions are caught at request handler boundaries and converted into JSON-RPC errors.

## 8. Modularity and Extensibility

*   **Registration System:** 
    *   Tools, resources, and prompts are registered via `FUMCP_Server::RegisterTool()`, `RegisterResource()`, `RegisterResourceTemplate()`, and `RegisterPrompt()`.
    *   Registration happens during module startup in `FUnrealMCPServerModule::StartupModule()`.
*   **Common Implementations:**
    *   `FUMCP_CommonTools`: Registers and implements general-purpose MCP tools (project config, console commands).
    *   `FUMCP_AssetTools`: Registers and implements asset-related MCP tools (search, export, import, query).
    *   `FUMCP_CommonResources`: Registers and implements common MCP resources for asset access.
    *   Additional tool/resource/prompt providers can be added by creating similar classes and registering them in the module.
*   **Method Handler Registration:**
    *   Custom JSON-RPC methods can be registered via `FUMCP_Server::RegisterRpcMethodHandler()`.
    *   Handlers use the `UMCP_JsonRpcHandler` function type.
*   **Configuration:**
    *   HTTP server port is configurable via `FUMCP_Server::HttpServerPort` (default: 30069).
    *   Configuration via INI files is not yet implemented but can be added.

## 9. Implementation Status

### ✅ Completed (Phase 1-3)

1.  **Core HTTP Server & JSON-RPC Framework**
    *   ✅ HTTP server using `FHttpServerModule` with `/mcp` endpoint.
    *   ✅ JSON-RPC 2.0 message parsing and serialization (core USTRUCTs).
    *   ✅ `initialize` handshake with protocol version "2024-11-05".
    *   ✅ `notifications/initialized` handling.
    *   ✅ `ping` utility.
    *   ✅ Method routing system with handler registration.

2.  **Tool Implementation**
    *   ✅ `tools/list` and `tools/call` (synchronous responses).
    *   ✅ 8 tools implemented: 6 asset tools (search_blueprints, export_asset, export_class_default, import_asset, query_asset, search_assets) and 2 common tools (get_project_config, execute_console_command).
    *   ✅ Tools organized into `FUMCP_AssetTools` (asset operations) and `FUMCP_CommonTools` (general-purpose operations).
    *   ✅ JSON Schema generation from USTRUCTs for tool input/output.
    *   ✅ Structured output support for tools with outputSchema.
    *   ✅ Type-safe parameter handling using USTRUCTs.

3.  **Resource Implementation**
    *   ✅ `resources/list`, `resources/templates/list`, and `resources/read`.
    *   ✅ URI template system (RFC 6570) for dynamic resource discovery.
    *   ✅ Blueprint T3D exporter as resource template (`unreal+t3d://{filepath}`).
    *   ✅ Static resource registration support.

4.  **Prompt Framework**
    *   ✅ `prompts/list` and `prompts/get` implementation.
    *   ✅ Prompt registration system.
    *   ⚠️ No prompts currently registered (framework ready for use).

### ⏳ Deferred (Future Phases)

*   **SSE/Streaming Support:**
    *   ⏳ Server-Sent Events (SSE) for streaming responses.
    *   ⏳ HTTP GET endpoint for server-initiated streams.
    *   ⏳ `notifications/tools/list_changed`.
    *   ⏳ `notifications/resources/content_changed`.
    *   ⏳ `resources/subscribe`.
    *   ⏳ `$/progress` for long-running operations.

*   **Additional Features:**
    *   ⏳ `shutdown` and `exit` methods.
    *   ⏳ `$/cancelRequest` for operation cancellation.
    *   ⏳ Session management with `Mcp-Session-Id`.
    *   ⏳ TLS (HTTPS) support.
    *   ⏳ Configuration via INI files.
    *   ⏳ Additional prompts.

## 10. Known Limitations & Future Work

*   **`FHttpServerModule` Suitability:** 
    *   Currently using `FHttpServerModule` which works well for basic HTTP POST requests.
    *   SSE support may require additional investigation or a switch to a third-party library.
*   **HTTPS Implementation:** 
    *   TLS/HTTPS is not yet implemented. Server operates over HTTP only.
    *   This is acceptable for local development but must be addressed for production use.
*   **Session Management:** 
    *   Currently stateless - no session IDs or persistent client state.
    *   This is acceptable for the current implementation but may need enhancement for advanced features.
*   **Performance:** 
    *   All tool/resource operations are marshaled to the game thread, which could impact performance with many concurrent requests.
    *   Large asset exports (e.g., T3D) may produce large JSON responses.
*   **Schema Evolution:** 
    *   USTRUCTs are manually maintained to match the MCP schema.
    *   Protocol version is currently "2024-11-05" - may need updates for newer MCP versions.
*   **Error Handling:** 
    *   Some edge cases may not be fully handled (e.g., malformed URIs, invalid asset paths).
    *   More comprehensive error messages could be added.

## 11. Future Considerations (Post-MVP)

*   **SSE/Streaming:**
    *   Implement SSE components of Streamable HTTP (`Content-Type: text/event-stream` responses, client GET support).
    *   Implement `$/progress`, `notifications/tools/list_changed`, `notifications/resources/content_changed`, and `resources/subscribe`.
*   **Additional MCP Features:**
    *   Server-initiated Sampling (`sampling/createMessage`).
    *   Client-exposed Roots (`roots/list`).
*   **Security:**
    *   TLS (HTTPS) implementation.
    *   Authentication/authorization mechanisms (API keys, OAuth, etc.).
    *   Origin header validation.
*   **Performance:**
    *   Async tool execution on worker threads where appropriate.
    *   Response caching for frequently accessed resources.
    *   Request queuing and rate limiting.
*   **Configuration:**
    *   INI file configuration for port, enabled features, etc.
    *   Editor UI for server configuration.
*   **Additional Tools/Resources:**
    *   More asset manipulation tools.
    *   Additional resource types (textures, materials, levels, etc.).
    *   Game state access resources.
*   **Testing:**
    *   Unit tests for JSON-RPC handling.
    *   Integration tests with MCP clients.
    *   Performance benchmarks.

