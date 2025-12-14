# UnrealMCPProxy Testing Plan

This document outlines the comprehensive testing strategy for the UnrealMCPProxy subproject. All tests use the `uv` tool to ensure compatibility with the production environment.

## Table of Contents

1. [Overview](#overview)
2. [Test Organization](#test-organization)
3. [Running Tests](#running-tests)
4. [Test Phases](#test-phases)
5. [Test Maintenance](#test-maintenance)
6. [Continuous Integration](#continuous-integration)

## Overview

The UnrealMCPProxy testing strategy is organized into **7 phases**, progressing from basic offline validation to comprehensive integration testing:

1. **Phase 1: Offline Validation** - Tests that work without backend
2. **Phase 2: Backend Connection** - Tests requiring backend connection
3. **Phase 3: Tool Calls** - Tests for tool call functionality
4. **Phase 4: Schema Compatibility** - Tool definition validation
5. **Phase 5: Error Paths and Edge Cases** - Error handling validation
6. **Phase 6: Resources** - Resource and resource template functionality
7. **Phase 7: Prompts** - Prompt functionality

### Prerequisites

- Python 3.12+ installed
- `uv` tool installed and available in PATH
- (Optional) Unreal Editor with UnrealMCPServer plugin enabled for online tests

## Test Organization

### Test Directory Structure

```
UnrealMCPProxy/
├── tests/
│   ├── __init__.py
│   ├── test_integration.py      # Main integration test suite
│   ├── test_unit.py             # Unit tests with mocks
│   ├── run_test_phases.py       # Phased test runner
│   ├── test_proxy_basic.py      # DEPRECATED (kept for reference)
│   └── test_server_integration.py  # DEPRECATED (kept for reference)
├── run_tests.py                 # Main test runner (runs all tests)
└── docs/
    ├── TESTING_PLAN.md          # This document
    └── QUICK_TEST_REFERENCE.md  # Quick test reference guide
```

### Test Files

- **`tests/test_integration.py`**: Main integration test suite with all test categories
- **`tests/test_unit.py`**: Unit tests with mocks (no backend required)
- **`tests/run_test_phases.py`**: Phased test runner for organized testing
- **`run_tests.py`**: Main entry point that runs all test suites

### Deprecated Files

- **`tests/test_proxy_basic.py`**: Deprecated - tests consolidated into `test_integration.py`
- **`tests/test_server_integration.py`**: Deprecated - tests consolidated into `test_integration.py`

These files are kept for reference but should not be used for new tests.

## Running Tests

### Quick Start

```bash
# Run all tests (recommended)
uv --directory . run run_tests.py

# Run phased tests
uv --directory . run tests/run_test_phases.py --all --skip-offline

# Run unit tests only
uv --directory . run tests/run_test_phases.py --unit
```

### Test Runners

**Main Test Runner** (`run_tests.py`):
```bash
uv --directory . run run_tests.py
```
Runs the complete consolidated test suite (integration + unit tests).

**Phased Test Runner** (`tests/run_test_phases.py`):
```bash
# Run all phases
uv --directory . run tests/run_test_phases.py --all --skip-offline

# Run specific phase
uv --directory . run tests/run_test_phases.py --phase 1

# Run multiple phases
uv --directory . run tests/run_test_phases.py --phases 1 2 3

# Run unit tests
uv --directory . run tests/run_test_phases.py --unit
```

**Direct Integration Tests**:
```bash
# Run integration tests directly
uv --directory . run python -m pytest tests/test_integration.py -v

# Run unit tests directly
uv --directory . run python -m pytest tests/test_unit.py -v
```

### Important Notes

- **Always use `uv`**: All tests must be run using `uv --directory .` to ensure compatibility with production
- **Backend availability**: Some tests require Unreal Editor to be running with UnrealMCPServer plugin enabled
- **Offline tests**: Phase 1 and unit tests work without backend connection

## Test Phases

### Phase 1: Offline Validation

**Goal**: Validate that the proxy can function independently without a backend connection.

**Tests**:
- `test_all_tools_defined`: Verifies all 15 expected tools are defined
- `test_prompt_definitions_file`: Verifies prompts.json can be found and read
- `test_resource_definitions_file`: Verifies resources.json can be found and read
- `test_cached_tool_definitions`: Validates cached tool definitions are available
- `test_error_invalid_tool`: Tests error handling for invalid tools
- `test_error_offline_backend`: Tests error handling when backend is offline
- `test_server_module_import`: Verifies server module can be imported without errors or deprecation warnings (as module)
- `test_server_module_import_as_script`: Verifies server module can be imported when run as script (absolute imports)

**Run**:
```bash
uv --directory . run tests/run_test_phases.py --phase 1
```

**Expected Results**:
- All 15 expected tools are defined (4 Common, 9 Asset, 2 Blueprint)
- Prompt definitions JSON file can be found and parsed
- Resource definitions JSON file can be found and parsed
- Expected prompts and resource templates are present in JSON files
- Tool definitions are properly structured
- Error handling works correctly

### Phase 2: Backend Connection

**Goal**: Validate proxy functionality when backend is available.

**Prerequisites**: Unreal Editor must be running with UnrealMCPServer plugin enabled.

**Tests**:
- `test_backend_connection`: Tests backend connection detection
- `test_tool_list`: Tests retrieving tool list from backend

**Run**:
```bash
uv --directory . run tests/run_test_phases.py --phase 2
```

**Expected Results**:
- Backend connection is established
- Tool list is retrieved successfully (15 tools)
- Connection state is detected immediately

### Phase 3: Tool Calls

**Goal**: Validate tool call functionality.

**Prerequisites**: Backend must be online.

**Tests**:
- `test_get_project_config`: Tests calling `get_project_config` tool
- `test_client_direct`: Tests direct client tool calls

**Run**:
```bash
uv --directory . run tests/run_test_phases.py --phase 3
```

**Expected Results**:
- Tool calls succeed
- Responses are properly formatted
- Tool results are correctly parsed

### Phase 4: Schema Compatibility

**Goal**: Validate that proxy tool definitions are compatible with backend.

**Prerequisites**: Backend must be online.

**Tests**:
- `test_schema_comparison`: Comprehensive schema compatibility check
- `test_description_matching`: Tool description matching validation

**Run**:
```bash
uv --directory . run tests/run_test_phases.py --phase 4
```

**Expected Results**:
- All tools are schema-compatible
- No missing required fields
- Type compatibility is maintained
- No compatibility warnings

### Phase 5: Error Paths and Edge Cases

**Goal**: Validate error handling and edge cases.

**Prerequisites**: Backend must be online (for some tests).

**Tests**:
- `test_edge_case_empty_arguments`: Tests tool calls with empty arguments
- Additional error path tests (invalid tools, offline backend, etc.)

**Run**:
```bash
uv --directory . run tests/run_test_phases.py --phase 5
```

**Expected Results**:
- Error paths are handled gracefully
- Edge cases are properly handled
- No crashes or unexpected exceptions

### Phase 6: Resources

**Goal**: Validate resource functionality.

**Prerequisites**: Backend must be online.

**Tests**:
- `test_test_assets_exist`: Verifies plugin test assets can be queried (validates test infrastructure)
- `test_resources_list`: Tests retrieving static resources list
- `test_resource_templates_list`: Tests retrieving resource templates list (T3D and Markdown)
- `test_read_resource`: Tests reading a resource by URI

**Test Assets**: Tests use plugin test assets located at `/UnrealMCPServer/TestAssets/Blueprints/`. These assets are part of the plugin and should be available when the plugin is installed. See `Content/TestAssets/README.md` for details on creating test assets.

**Run**:
```bash
uv --directory . run tests/run_test_phases.py --phase 6
```

**Expected Results**:
- Resources list is retrieved successfully
- Resource templates are available (unreal+t3d://{filepath}, unreal+md://{filepath})
- Resource reading works (or handles missing resources gracefully)

### Phase 7: Prompts

**Goal**: Validate prompt functionality.

**Prerequisites**: Backend must be online.

**Tests**:
- `test_prompts_list`: Tests retrieving prompts list
- `test_get_prompt`: Tests getting a prompt with arguments

**Run**:
```bash
uv --directory . run tests/run_test_phases.py --phase 7
```

**Expected Results**:
- Prompts list is retrieved successfully
- Expected prompts are available (analyze_blueprint, refactor_blueprint, etc.)
- Prompt retrieval works with arguments

## Test Maintenance

### When to Update Tests

Tests should be updated when:

1. **Adding new tools**: Add tests for new tool functionality
2. **Modifying tool definitions**: Update schema compatibility tests
3. **Changing response formats**: Update response parsing tests
4. **Refactoring code**: Ensure tests still cover all functionality
5. **Fixing bugs**: Add regression tests for fixed bugs
6. **Server startup changes**: Update server import tests if startup logic changes
7. **Deprecation warnings**: Ensure server import tests catch new deprecation warnings

### Adding New Tests

1. **Integration tests**: Add to `tests/test_integration.py` in the appropriate category
2. **Unit tests**: Add to `tests/test_unit.py` with appropriate mocks
3. **Test organization**: Follow existing patterns and categories
4. **Test naming**: Use descriptive names starting with `test_`

### Test Categories in `test_integration.py`

- **Category 1**: Tool Definition Tests (offline)
  - Includes tests for JSON definition files (prompts.json, resources.json)
- **Category 2**: Backend Connection Tests (online)
- **Category 3**: Tool Call Tests (online)
- **Category 4**: Schema Compatibility Tests (online)
- **Category 5**: Error Path and Edge Case Tests
- **Category 6**: Resources Tests (online)
- **Category 7**: Prompts Tests (online)

### Best Practices

1. **Use shared fixtures**: Use `create_test_client()` and `cleanup_client()` helpers
2. **Handle offline gracefully**: Tests should not fail if backend is offline (unless testing online functionality)
3. **Clear error messages**: Provide descriptive error messages in test failures
4. **Test isolation**: Each test should be independent and not rely on other tests
5. **Use `uv` tool**: Always run tests with `uv --directory .` to match production

## Continuous Integration

### CI/CD Pipeline

For CI/CD pipelines, run tests in this order:

```bash
# Phase 1: Offline validation (always runs)
uv --directory . run tests/run_test_phases.py --phase 1

# Unit tests (always runs)
uv --directory . run tests/run_test_phases.py --unit

# Phases 2-7: Integration tests (only if backend available)
if [ "$BACKEND_AVAILABLE" = "true" ]; then
    uv --directory . run tests/run_test_phases.py --phases 2 3 4 5 6 7
fi
```

**Windows PowerShell**:
```powershell
# Phase 1: Offline validation (always runs)
uv --directory . run tests/run_test_phases.py --phase 1

# Unit tests (always runs)
uv --directory . run tests/run_test_phases.py --unit

# Phases 2-7: Integration tests (only if backend available)
if ($env:BACKEND_AVAILABLE -eq "true") {
    uv --directory . run tests/run_test_phases.py --phases 2 3 4 5 6 7
}
```

### Success Criteria

- **Phase 1**: All 4 offline validation tests pass
- **Unit Tests**: All 21 unit tests pass
- **Phase 2**: Backend connection and tool list retrieval work
- **Phase 3**: Tool calls succeed and return correct results
- **Phase 4**: All 15 tools are schema-compatible (0 compatibility issues)
- **Phase 5**: Error paths and edge cases are handled correctly
- **Phase 6**: Resources and resource templates are accessible
- **Phase 7**: Prompts are accessible and functional

### Test Coverage Goals

- **Tool definitions**: 100% coverage (all 15 tools tested)
- **Schema compatibility**: 100% coverage (all tools validated)
- **Error handling**: All error paths tested
- **Edge cases**: Common edge cases covered

## Troubleshooting

### Common Issues

**Issue**: Tests fail with import errors
- **Solution**: Ensure you're using `uv --directory .` to run tests
- **Solution**: Check that `src/` directory is in the path

**Issue**: Server startup fails with deprecation warnings
- **Solution**: Run `test_server_module_import` and `test_server_module_import_as_script` to identify warnings
- **Solution**: Update server.py to use modern APIs (e.g., `asyncio.get_running_loop()` instead of `asyncio.get_event_loop()`)

**Issue**: Backend connection fails
- **Solution**: Ensure Unreal Editor is running
- **Solution**: Verify UnrealMCPServer plugin is enabled
- **Solution**: Check backend port (default: 30069)

**Issue**: Schema compatibility warnings
- **Solution**: Update proxy tool definitions to match backend
- **Solution**: Review `docs/SCHEMA_SYNCHRONIZATION_GUIDE.md`

**Issue**: Tests timeout
- **Solution**: Increase timeout in configuration
- **Solution**: Check backend is responding

**Issue**: Unicode encoding errors
- **Solution**: Avoid emoji characters in test output (use `[OK]` instead)

## Quick Reference

See `QUICK_TEST_REFERENCE.md` for a quick reference guide with common test commands.

## Related Documentation

- `../README.md`: Project overview and usage
- `../../docs/SCHEMA_SYNCHRONIZATION_GUIDE.md`: Schema compatibility requirements (in plugin root docs/)
- `QUICK_TEST_REFERENCE.md`: Quick reference for test commands
- `TESTING_SUMMARY.md`: Quick overview of testing structure
