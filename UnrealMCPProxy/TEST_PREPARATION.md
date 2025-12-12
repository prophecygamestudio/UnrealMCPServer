# UnrealMCPProxy Test Preparation Checklist

## Prerequisites

**No manual installation needed!** `uv run` automatically:
- Creates a virtual environment (`.venv`)
- Installs all dependencies from `requirements.txt`
- Ensures a compatible environment with end users

This matches the production execution method used in `mcp.json` configuration.

**Optional Configuration:**
- Copy `.env.example` to `.env` if you want to customize settings
- Default settings work for basic testing

## Test Files

### `test_proxy.py` - Main Test Script
Validates:
- ✅ All 15 tools are defined in cache
- ✅ Backend connection (if Unreal Editor is running)
- ✅ `get_project_config` tool call (if backend online)
- ✅ Tool description matching between cache and backend

**Usage:**
```bash
uv --directory . run test_proxy.py
```

**Expected Behavior:**
- Runs successfully even if backend is offline
- Validates all tool definitions are present
- Tests tool calls if backend is available
- Reports any mismatches between cache and backend

### `tests/test_proxy_basic.py` - Comprehensive Test Suite
More detailed tests for individual components.

### `tests/test_server_integration.py` - Integration Tests
Tests server startup and basic functionality.

## Running Tests

### Quick Test (Recommended)
```bash
cd UnrealMCPProxy
uv --directory . run test_proxy.py
```

**Important**: Always use `uv --directory . run`, not `python` directly, to ensure a compatible environment.

### Full Test Suite
```bash
cd UnrealMCPProxy
uv --directory . run run_tests.py
```

## Test Scenarios

### Scenario 1: Backend Offline (Default)
- ✅ All tools defined test should pass
- ⚠ Backend connection test will skip
- ⚠ Tool call tests will skip
- ✅ Description matching will skip (expected)

### Scenario 2: Backend Online
- ✅ All tools defined test should pass
- ✅ Backend connection test should pass
- ✅ Tool call test should pass (if backend responds correctly)
- ✅ Description matching should pass (or show warnings for mismatches)

## Expected Test Results

### All Tools Defined Test
Should show:
```
✓ All 15 expected tools are defined
  Common tools: 4
  Asset tools: 9
  Blueprint tools: 2
```

### Tool Description Matching Test
When backend is online:
- Lists all tools from backend
- Compares each with cached definition
- Shows matches and mismatches
- Logs warnings for any differences

## Troubleshooting

### ModuleNotFoundError: No module named 'httpx'
**Solution**: This should not occur when using `uv run`, as it automatically installs dependencies. If you see this error:
- Ensure you're using `uv --directory . run test_proxy.py` (not `python test_proxy.py`)
- `uv` will automatically create a virtual environment and install all dependencies from `requirements.txt`

### Backend Connection Failed
**Expected**: If Unreal Editor is not running, this is normal. Tests will skip backend-dependent tests.

### Tool Definition Mismatches
**Action**: If mismatches are detected, update `tool_definitions.py` to match the backend definitions. See `.cursorrules` for synchronization requirements.

## Next Steps After Tests

1. **If all tests pass**: Proxy is ready for use
2. **If mismatches detected**: Update tool definitions in `tool_definitions.py`
3. **If missing tools**: Add missing tool definitions
4. **If backend errors**: Check Unreal Editor is running and plugin is enabled

