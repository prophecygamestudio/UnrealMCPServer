# UnrealMCPProxy Test Suite

This directory contains all test files for the UnrealMCPProxy project.

## Test Files

### Active Test Files

- **`test_integration.py`**: Main integration test suite
  - Consolidates all integration tests
  - Organized by category (Tool Definitions, Backend Connection, Tool Calls, Schema Compatibility, Error Paths)
  - Tests require backend connection (some work offline)

- **`test_unit.py`**: Unit tests with mocks
  - Tests core functions in isolation
  - Uses mocks - no backend connection required
  - Fast execution, suitable for CI/CD

- **`run_test_phases.py`**: Phased test runner
  - Runs tests organized by phase (1-5)
  - Supports running specific phases or all phases
  - Handles offline/online scenarios gracefully

### Deprecated Test Files

- **`test_proxy_basic.py`**: DEPRECATED
  - Tests consolidated into `test_integration.py`
  - Kept for reference only
  - Do not use for new tests

- **`test_server_integration.py`**: DEPRECATED
  - Tests consolidated into `test_integration.py`
  - Kept for reference only
  - Do not use for new tests

## Running Tests

### From Project Root

```bash
# Run all tests
uv --directory . run run_tests.py

# Run phased tests
uv --directory . run tests/run_test_phases.py --all --skip-offline

# Run specific phase
uv --directory . run tests/run_test_phases.py --phase 1

# Run unit tests
uv --directory . run tests/run_test_phases.py --unit
```

### Direct Execution

```bash
# Integration tests
uv --directory . run python -m pytest tests/test_integration.py -v

# Unit tests
uv --directory . run python -m pytest tests/test_unit.py -v
```

## Test Organization

### Integration Tests (`test_integration.py`)

Tests are organized into 5 categories:

1. **Tool Definition Tests** (Category 1)
   - `test_all_tools_defined`
   - `test_cached_tool_definitions`

2. **Backend Connection Tests** (Category 2)
   - `test_backend_connection`
   - `test_tool_list`

3. **Tool Call Tests** (Category 3)
   - `test_get_project_config`
   - `test_client_direct`

4. **Schema Compatibility Tests** (Category 4)
   - `test_schema_comparison`
   - `test_description_matching`

5. **Error Path and Edge Case Tests** (Category 5)
   - `test_error_invalid_tool`
   - `test_error_offline_backend`
   - `test_edge_case_empty_arguments`

### Unit Tests (`test_unit.py`)

Unit tests cover:
- Tool definition comparison
- Read-only tool detection
- Error handling
- Retry logic
- Tool decorators

## Adding New Tests

### Integration Tests

Add new tests to `test_integration.py` in the appropriate category:

```python
async def test_my_new_feature() -> bool:
    """Test description."""
    print("=" * 60)
    print("Test: My New Feature")
    print("=" * 60)
    
    client = await create_test_client()
    try:
        # Test implementation
        return True
    finally:
        await cleanup_client(client)
```

### Unit Tests

Add new unit tests to `test_unit.py`:

```python
def test_my_new_function():
    """Test description."""
    # Test implementation with mocks
    assert result == expected
```

## Test Helpers

### Shared Fixtures

- `create_test_client()`: Creates a test client instance
- `cleanup_client(client)`: Cleans up a test client
- `parse_tool_response(response)`: Parses tool response format

### Constants

- `EXPECTED_TOOLS`: List of all expected tool names (15 tools)

## Best Practices

1. **Use shared fixtures**: Always use `create_test_client()` and `cleanup_client()`
2. **Handle offline gracefully**: Tests should not fail if backend is offline (unless testing online functionality)
3. **Clear test names**: Use descriptive names starting with `test_`
4. **Test isolation**: Each test should be independent
5. **Use `uv` tool**: Always run tests with `uv --directory .`

## Documentation

- See `../docs/TESTING_PLAN.md` for comprehensive testing documentation
- See `../docs/QUICK_TEST_REFERENCE.md` for quick command reference
- See `../docs/TESTING_SUMMARY.md` for quick overview

