# Project Checklist: UnrealMCPServer - MCP Server (HTTP Implementation)

**Overall Goal:** Create an Unreal Engine plugin (`UnrealMCPServer`) that implements a Model Context Protocol (MCP) server using synchronous HTTP POST for requests/responses, exposing UE capabilities.

**Note:** Server-Sent Events (SSE) and server-initiated notifications are **deferred** for initial implementation. Communication will be client-initiated request (POST) and server response (in HTTP response body).

---

## Phase 1: Core HTTP Server & JSON-RPC Framework
- [X] **Task 1.1:** Set up the chosen HTTP server (e.g., `FHttpServerModule` PoC) to handle synchronous POST requests.
    - [X] Sub-task: Configure a listening port (e.g., 30069) for HTTP.
    - [X] Sub-task: Implement basic request routing for a `/mcp` endpoint.
- [X] **Task 1.2:** Implement core JSON-RPC message parsing and serialization.
    - [X] Sub-task: Define USTRUCTs for `FJsonRpcRequest`, `FJsonRpcResponse`, `FJsonRpcError`.
    - [X] Sub-task: Implement functions to convert between JSON strings and these USTRUCTs.
- [X] **Task 1.3:** Implement the `initialize` MCP method (synchronous request/response).
    - [X] Sub-task: Define `FInitializeParams` and `FInitializeResult` USTRUCTs.
    - [X] Sub-task: Define `FServerInfo` and `FServerCapabilities` USTRUCTs.
    - [X] Sub-task: Handle client `initialize` POST request and return `FInitializeResult` in HTTP response.
- [X] **Task 1.4:** Implement handling for the `notifications/initialized` MCP client notification.
    - [X] Sub-task: Server receives POST, processes, returns HTTP 204 or simple success.
- [X] **Task 1.5:** Implement a basic session/state management concept (even if single-client focused for now).
    - [X] Sub-task: Store negotiated capabilities after `initialize`.
- [X] **Task 1.6:** Implement the `ping` utility MCP method (synchronous request/response).
- [X] **Task 1.7:** Basic logging setup (e.g., `LogUnrealMCPServer` category).
- [X] **Status:** Completed

## Phase 2: Basic Tool Implementation
- [X] **Task 2.1:** Implement `tools/list` JSON-RPC method (synchronous response).
    - [X] Sub-task: Define `FToolDefinition` USTRUCT.
    - [X] Sub-task: Implement tool discovery/registration mechanism (e.g., manual registration of C++ tools).
- [X] **Task 2.2:** Implement `tools/call` JSON-RPC method (synchronous response).
    - [X] Sub-task: Define `FToolCallParams` and `FToolCallResult` USTRUCTs.
    - [X] Sub-task: Define `FContentPart` USTRUCTs (Text, Image initially).
    - [X] Sub-task: Implement tool execution logic (C++ focused, callable from MCP client via POST).
    - [X] Sub-task: Tool execution runs on game thread (marshaled from HTTP thread), with the final result returned in the HTTP response.
- [X] **Task 2.3:** Create example C++ tools.
    - [X] Implemented 7 tools: search_blueprints, export_asset, export_class_default, import_asset, query_asset, search_assets, get_project_config.
- [ ] ~~**Task 2.4:** Implement `notifications/tools/list_changed` server notification.~~ **(Deferred, requires SSE)**
- [X] **Status:** Completed

## Phase 3: Basic Resource Implementation
- [X] **Task 3.1:** Implement `resources/list` JSON-RPC method (synchronous response).
    - [X] Sub-task: Define `FResourceDefinition` USTRUCT.
    - [X] Sub-task: Implement resource discovery/registration mechanism.
- [X] **Task 3.2:** Implement `resources/read` JSON-RPC method (synchronous response).
    - [X] Sub-task: Define `FReadResourceParams` and `FReadResourceResult` USTRUCTs.
    - [X] Sub-task: Implement logic to retrieve resource content (e.g., T3D for a Blueprint).
- [X] **Task 3.3:** Implement resource templates with URI template support (`resources/templates/list`).
    - [X] Implemented Blueprint T3D exporter as resource template (`unreal+t3d://{filepath}`).
- [ ] ~~**Task 3.4:** Implement `notifications/resources/content_changed` server notification.~~ **(Deferred, requires SSE)**
- [X] **Status:** Completed

## Phase 4: Advanced Features & Refinements (Non-SSE)
- [X] **Task 4.1:** Implement Prompts (`prompts/list`, `prompts/get`) (synchronous responses).
    - [X] Sub-task: Define `FPromptDefinition`, `FGetPromptParams`, `FGetPromptResult` USTRUCTs.
    - [X] Sub-task: Prompt framework implemented (no prompts currently registered).
- [ ] **Task 4.2:** Implement `$/cancelRequest` client notification.
    - [ ] Sub-task: Server attempts to honor cancellation for ongoing asynchronous tasks.
- [X] **Task 4.3:** Robust error handling and reporting as JSON-RPC errors in HTTP responses.
- [X] **Task 4.4:** Threading implementation (HTTP requests on server threads, UE access on game thread).
- [ ] **Task 4.5 (Deferred):** Resource subscriptions (`resources/subscribe`, `notifications/resources/content_changed`). **(Deferred, requires SSE)**
- [ ] **Task 4.6 (Deferred):** Implement `$/progress` server notification for long-running tool calls. **(Deferred, requires SSE)**
- [X] **Status:** Partially Completed (Prompts framework done, error handling done, threading done)

## Phase 5: Security & Deployment
- [ ] **Task 5.1:** Implement TLS (HTTPS) for secure communication. **(Deferred for initial implementation; HTTP will be used locally)**
    - [ ] Sub-task: Evaluate `FHttpServerModule` for HTTPS or select/integrate third-party library (for future implementation).
- [ ] **Task 5.2:** Implement configuration options (port, features via INI/Editor Settings).
    - [X] Sub-task: Port is configurable via code (`HttpServerPort` member variable).
    - [ ] Sub-task: INI file configuration not yet implemented.
- [ ] **Task 5.3:** Plugin packaging and end-to-end testing with a sample MCP client.
- [X] **Task 5.4:** Documentation for using the plugin.
    - [X] README.md with installation and usage instructions.
    - [X] Technical specification document.
- [X] **Status:** Partially Completed (Documentation done, basic configuration available)

## Future Considerations (Post-Initial Implementation)
- [ ] Implement Server-Sent Events (SSE) components of Streamable HTTP (including `Content-Type: text/event-stream` responses, client GET support for server-initiated streams, and features like `notifications/tools/list_changed`, `notifications/resources/content_changed`, `$/progress`, and `resources/subscribe`).
- [ ] Implement Server-initiated Sampling (`sampling/createMessage`).
- [ ] Implement Client-exposed Roots (`roots/list`).
- [ ] Explore advanced tool output (structured content beyond text/image).
- [ ] Investigate more robust session management for multi-client scenarios.
- [ ] Performance optimizations for high-throughput scenarios.