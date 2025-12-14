# UnrealMCPProxy

Stable frontend service for Unreal Engine MCP Server. Provides persistent availability and graceful error handling when the Unreal Editor is not running or restarting.

**Note**: This is not a network proxy, but rather a stable frontend service designed to provide a reliable interface for AI agents against an unreliable backend state.

## Status

✅ **Implementation Complete** - The proxy is fully functional and ready for use. See `IMPLEMENTATION_COMPLETE.md` for details.

## Overview

**Important**: The UnrealMCPProxy is **not a network proxy**. It is a **stable frontend service** designed to provide a reliable interface for AI agents against an unreliable backend state (the Unreal Editor, which may restart or be unavailable).

The UnrealMCPProxy acts as a stable frontend between MCP clients (like Cursor, Claude, etc.) and the Unreal Engine MCP Server. It:

- **Stays alive independently** - Runs as a separate process, independent of Unreal Editor
- **Monitors backend availability** - Periodically checks if the Unreal MCP server is online
- **Forwards requests** - Forwards MCP requests to the backend when available
- **Returns graceful errors** - Provides clear error messages when backend is unavailable
- **Provides offline tool lists** - Can return tool definitions even when backend is offline
- **Default STDIO transport** - Supports multiple agents connecting to the same editor

The proxy's primary purpose is to maintain availability and provide consistent tool definitions even when the Unreal Editor is restarting or temporarily unavailable, ensuring AI agents have a stable interface to interact with.

## Installation

**No manual installation required!** The proxy uses `uv` for dependency management. When you run the proxy or tests with `uv run`, it automatically:
- Creates a virtual environment (`.venv`)
- Installs all dependencies from `requirements.txt`
- Ensures a compatible environment with end users

This matches the production execution method used in `mcp.json` configuration.

## Configuration

Configuration is done via environment variables (see `.env.example`). The `.env` file is optional - defaults work for most setups.

**Configuration options:**
- **Backend Configuration**: `UNREAL_MCP_PROXY_BACKEND_HOST`, `UNREAL_MCP_PROXY_BACKEND_PORT`, `UNREAL_MCP_PROXY_BACKEND_TIMEOUT`
- **Health Check**: `UNREAL_MCP_PROXY_HEALTH_CHECK_INTERVAL`, `UNREAL_MCP_PROXY_HEALTH_CHECK_START_ON_FIRST_CALL`
- **Retry Settings**: `UNREAL_MCP_PROXY_RETRY_MAX_ATTEMPTS`, `UNREAL_MCP_PROXY_RETRY_INITIAL_DELAY`, `UNREAL_MCP_PROXY_RETRY_MAX_DELAY`, `UNREAL_MCP_PROXY_RETRY_BACKOFF_FACTOR`
- **Proxy Server**: `UNREAL_MCP_PROXY_HOST`, `UNREAL_MCP_PROXY_PORT`, `UNREAL_MCP_PROXY_TRANSPORT`
- **Conditional Features**: `UNREAL_MCP_PROXY_ENABLE_MARKDOWN_EXPORT` (default: `true`) - Controls markdown export tools and markdown resource support. Set to `false` if BP2AI plugin is not installed.
- **Development**: `UNREAL_MCP_PROXY_DEBUG` (enables debug logging)
- **Logging**: `UNREAL_MCP_PROXY_LOG_LEVEL`, `UNREAL_MCP_PROXY_LOG_FILE`, `UNREAL_MCP_PROXY_LOG_FORMAT`

All settings have validation and sensible defaults. Invalid values will raise clear errors at startup.

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

### Quick Start

```bash
# Run all tests (recommended)
uv --directory . run run_tests.py

# Run phased tests
uv --directory . run tests/run_test_phases.py --all --skip-offline

# Run unit tests only
uv --directory . run tests/run_test_phases.py --unit
```

### Test Organization

- **`tests/test_integration.py`**: Main integration test suite with all test categories
- **`tests/test_unit.py`**: Unit tests with mocks (no backend required)
- **`tests/run_test_phases.py`**: Phased test runner for organized testing
- **`run_tests.py`**: Main entry point that runs all test suites

### Test Phases

The test suite is organized into 5 phases:

1. **Phase 1: Offline Validation** - Tool definitions, configuration, error handling (no backend required)
2. **Phase 2: Backend Connection** - Connection, health check, tool list (requires backend)
3. **Phase 3: Tool Calls** - Basic tool functionality (requires backend)
4. **Phase 4: Schema Compatibility** - Tool definition validation (requires backend)
5. **Phase 5: Error Paths** - Error handling and edge cases (requires backend)

### Running Specific Tests

**Integration Tests:**
```bash
uv --directory . run python -m pytest tests/test_integration.py -v
```

**Unit Tests:**
```bash
uv --directory . run python -m pytest tests/test_unit.py -v
```

**Specific Phase:**
```bash
uv --directory . run tests/run_test_phases.py --phase 1
```

**Important**: Tests must be run using `uv`, not Python directly, to ensure a compatible environment with end users.

**Note**: Some tests require Unreal Editor to be running. The script will skip those tests gracefully if the backend is offline.

### Testing Documentation

All testing documentation is located in the `docs/` directory:

- **`docs/TESTING_PLAN.md`**: Comprehensive testing plan and guidelines
- **`docs/QUICK_TEST_REFERENCE.md`**: Quick reference for test commands
- **`docs/TESTING_SUMMARY.md`**: Quick overview of testing structure
- **`tests/README.md`**: Test directory documentation

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

## Tool Definition Compatibility

**IMPORTANT**: The proxy maintains static tool definitions in `src/unreal_mcp_proxy/tool_definitions.py` that must be **compatible** with the UnrealMCPServer plugin.

Proxy schemas don't need to exactly match backend schemas. They must be **compatible**, meaning:
- All required backend fields must be present
- Field types must be compatible
- Proxy can have improved descriptions, simplified types, and better defaults

When backend tool definitions change (especially required fields), you should update the proxy definitions to maintain compatibility. See `.cursorrules` and `../docs/SCHEMA_SYNCHRONIZATION_GUIDE.md` for detailed requirements and best practices.

The proxy will log WARNING messages when it detects compatibility issues between proxy and backend tool definitions.

## Development

See `PROXY_IMPLEMENTATION_PLAN.md` and `PROXY_RESEARCH_SUMMARY.md` for detailed implementation information.

## Future Enhancements

See `FUTURE_ENHANCEMENTS.md` for planned improvements, including:
- Offline-capable Common tools (partial `get_project_config`, offline `request_editor_compile`)
- Enhanced error handling and recovery
- Additional configuration options

## License

See LICENSE file in parent directory.
