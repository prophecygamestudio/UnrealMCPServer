# Notebook for UnrealMCPServer

This notebook is for recording potentially useful information, findings, and tidbits related to the UnrealMCPServer plugin project.

## Initial Thoughts & Research

- **MCP Specification:** Review the official MCP specification (current implementation uses version "2024-11-05") for endpoint definitions and message structures.
- **Unreal FHttpServerModule:** Investigate usage examples and best practices for `FHttpServerModule` in Unreal Engine.
  - Key classes: `IHttpRouter`, `FHttpRouteHandle`, `FHttpServerRequest`, `FHttpServerResponse`.
- **JSON Parsing in Unreal:** Unreal uses `FJsonObject` and related classes for JSON manipulation. This will be crucial for MCP message processing.
- **Port:** 30069 (as specified by user).
- **Plugin Type:** Editor Plugin.
