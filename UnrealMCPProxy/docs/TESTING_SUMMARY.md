# Testing Summary

This document provides a quick overview of the testing structure and how to use it.

## Test Structure

```
UnrealMCPProxy/
├── tests/                          # All test files
│   ├── test_integration.py        # Main integration test suite
│   ├── test_unit.py               # Unit tests with mocks
│   ├── run_test_phases.py         # Phased test runner
│   ├── README.md                  # Test directory documentation
│   ├── test_proxy_basic.py       # DEPRECATED (reference only)
│   └── test_server_integration.py # DEPRECATED (reference only)
├── run_tests.py                   # Main test runner
├── docs/                          # Testing documentation
│   ├── TESTING_PLAN.md            # Comprehensive testing plan
│   ├── QUICK_TEST_REFERENCE.md    # Quick command reference
│   └── TESTING_SUMMARY.md         # This file
```

## Quick Commands

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

## Test Phases

1. **Phase 1**: Offline Validation (4 tests) - No backend required
2. **Phase 2**: Backend Connection (2 tests) - Requires backend
3. **Phase 3**: Tool Calls (2 tests) - Requires backend
4. **Phase 4**: Schema Compatibility (2 tests) - Requires backend
5. **Phase 5**: Error Paths (1 test) - Requires backend

**Plus**: 21 unit tests (no backend required)

## Test Results

All tests passing:
- ✅ 11 integration tests
- ✅ 21 unit tests
- ✅ 15/15 tools schema-compatible
- ✅ 0 compatibility issues

## Documentation

- **`docs/TESTING_PLAN.md`**: Full testing plan and guidelines
- **`docs/QUICK_TEST_REFERENCE.md`**: Quick command reference
- **`tests/README.md`**: Test directory documentation

