# UnrealMCPProxy

MCP proxy server for Unreal Engine MCP Server. Provides persistent availability and graceful error handling when the Unreal Editor is not running.

## Status

✅ **Implementation Complete** - The proxy is fully functional and ready for use. See `IMPLEMENTATION_COMPLETE.md` for details.

## Overview

The UnrealMCPProxy acts as an intermediary between MCP clients (like Cursor, Claude, etc.) and the Unreal Engine MCP Server. It:

- **Stays alive independently** - Runs as a separate process, independent of Unreal Editor
- **Monitors backend availability** - Periodically checks if the Unreal MCP server is online
- **Proxies requests** - Forwards MCP requests to the backend when available
- **Returns graceful errors** - Provides clear error messages when backend is unavailable
- **Provides offline tool lists** - Can return tool definitions even when backend is offline
- **Default STDIO transport** - Supports multiple agents connecting to the same editor

## Installation

**No manual installation required!** The proxy uses `uv` for dependency management. When you run the proxy or tests with `uv run`, it automatically:
- Creates a virtual environment (`.venv`)
- Installs all dependencies from `requirements.txt`
- Ensures a compatible environment with end users

This matches the production execution method used in `mcp.json` configuration.

## Configuration

Configuration is done via environment variables (see `.env.example`). The `.env` file is optional - defaults work for most setups.

**Configuration options:**
- **Backend Configuration**: `UNREAL_MCP_PROXY_BACKEND_HOST`, `UNREAL_MCP_PROXY_BACKEND_PORT`
- **Proxy Server**: `UNREAL_MCP_PROXY_HOST`, `UNREAL_MCP_PROXY_PORT`, `UNREAL_MCP_PROXY_TRANSPORT`
- **Conditional Features**: `UNREAL_MCP_PROXY_ENABLE_MARKDOWN_EXPORT`
- **Logging**: `UNREAL_MCP_PROXY_LOG_LEVEL`, `UNREAL_MCP_PROXY_LOG_FILE`

To customize configuration:
```bash
cp .env.example .env
# Edit .env with your settings
```

## Usage

### Running the Proxy

**Using `uv` (recommended, matches production configuration):**
```bash
uv --directory . run src/unreal_mcp_proxy/server.py
```

**Using Python directly (for development only):**
```bash
python -m src.unreal_mcp_proxy.server
```

**Using FastMCP CLI:**
```bash
mcp dev src/unreal_mcp_proxy/server.py
```

### MCP Client Configuration

Add to your `mcp.json`:

```json
{
  "mcpServers": {
    "unreal-mcp-proxy": {
      "command": "uv",
      "args": [
        "--directory",
        "/path/to/UnrealMCPProxy",
        "run",
        "src/unreal_mcp_proxy/server.py"
      ]
    }
  }
}
```

**Note**: Replace `/path/to/UnrealMCPProxy` with the absolute path to the UnrealMCPProxy directory on your system. The proxy defaults to STDIO transport, which allows multiple agents to connect to the same Unreal Editor instance.

### Transport Modes

The proxy supports multiple transport modes:
- **stdio** (default): Standard input/output - supports multiple agents connecting to the same editor
- **sse**: Server-Sent Events
- **http**: HTTP transport

Configure via `UNREAL_MCP_PROXY_TRANSPORT` environment variable.

## Testing

### Quick Test Script

Run the basic test script (validates functionality and description matching):

```bash
uv --directory . run test_proxy.py
```

**Important**: Tests must be run using `uv`, not Python directly, to ensure a compatible environment with end users.

This script:
- Tests backend connection
- Tests `get_project_config` tool call (if backend is online)
- Validates tool description matching between cache and backend
- Automatically exits upon completion

**Note**: Some tests require Unreal Editor to be running. The script will skip those tests gracefully if the backend is offline.

### Full Test Suite

Run the complete test suite:

```bash
uv --directory . run run_tests.py
```

Or run individual test files:

```bash
uv --directory . run -m tests.test_proxy_basic
uv --directory . run -m tests.test_server_integration
```

## Architecture

### Parameter Handling and Defaults

The proxy is the **authoritative source** for the user-facing API. It:

1. **Accepts optional parameters** from clients (matching the backend schema)
2. **Applies default values** for any missing optional parameters
3. **Explicitly passes ALL parameters** (including defaults) to the backend

This design ensures:
- **Better UX**: The proxy provides clear error messages and handles defaults
- **Backend flexibility**: Backend defaults serve as a safety net for direct backend calls
- **Explicit contracts**: All values are explicitly provided when forwarding requests
- **Type safety**: Backend maintains C++ type safety with struct defaults

The backend's default values are preserved for direct backend usage, but the proxy ensures complete parameter sets are always sent when forwarding requests.

## Tool Definition Synchronization

**IMPORTANT**: The proxy maintains static tool definitions in `src/unreal_mcp_proxy/tool_definitions.py` that must be kept in sync with the UnrealMCPServer plugin.

When tool descriptions are modified in the plugin, you MUST update the corresponding definitions in the proxy. See `.cursorrules` for detailed synchronization requirements.

The proxy will log WARNING messages when it detects mismatches between cached and backend tool definitions.

## Development

See `PROXY_IMPLEMENTATION_PLAN.md` and `PROXY_RESEARCH_SUMMARY.md` for detailed implementation information.

## Future Enhancements

See `FUTURE_ENHANCEMENTS.md` for planned improvements, including:
- Offline-capable Common tools (partial `get_project_config`, offline `request_editor_compile`)
- Enhanced error handling and recovery
- Additional configuration options

## License

See LICENSE file in parent directory.
