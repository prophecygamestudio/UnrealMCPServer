# Quick Test Reference

Quick reference guide for running UnrealMCPProxy tests using `uv`.

## Quick Start

### Run All Tests (Recommended)

```bash
# Run all tests (integration + unit)
uv --directory . run run_tests.py

# Run phased tests (all phases)
uv --directory . run tests/run_test_phases.py --all --skip-offline

# Run unit tests only
uv --directory . run tests/run_test_phases.py --unit
```

### Run Specific Phase

```bash
# Phase 1: Offline validation (no backend required)
uv --directory . run tests/run_test_phases.py --phase 1

# Phase 2: Backend connection (requires backend)
uv --directory . run tests/run_test_phases.py --phase 2

# Phase 3: Tool calls (requires backend)
uv --directory . run tests/run_test_phases.py --phase 3

# Phase 4: Schema compatibility (requires backend)
uv --directory . run tests/run_test_phases.py --phase 4

# Phase 5: Error paths (requires backend)
uv --directory . run tests/run_test_phases.py --phase 5

# Phase 6: Resources (requires backend)
uv --directory . run tests/run_test_phases.py --phase 6

# Phase 7: Prompts (requires backend)
uv --directory . run tests/run_test_phases.py --phase 7
```

### Run Multiple Phases

```bash
# Run phases 1, 2, and 3
uv --directory . run tests/run_test_phases.py --phases 1 2 3
```

## Alternative: Direct Test Execution

```bash
# Run integration tests directly
uv --directory . run python -m pytest tests/test_integration.py -v

# Run unit tests directly
uv --directory . run python -m pytest tests/test_unit.py -v

# Run all tests with pytest
uv --directory . run python -m pytest tests/ -v
```

## Test Phases Overview

| Phase | Name | Backend Required | Description |
|-------|------|------------------|-------------|
| 1 | Offline Validation | ❌ No | Tool definitions, JSON definition files, error handling |
| 2 | Backend Connection | ✅ Yes | Connection, health check, tool list |
| 3 | Tool Calls | ✅ Yes | Basic tool functionality |
| 4 | Schema Compatibility | ✅ Yes | Tool definition validation |
| 5 | Error Paths | ✅ Yes | Error handling and edge cases |
| 6 | Resources | ✅ Yes | Resource and resource template functionality |
| 7 | Prompts | ✅ Yes | Prompt functionality |

## Common Commands

```bash
# Quick offline validation
uv --directory . run tests/run_test_phases.py --phase 1

# Full validation (if backend available)
uv --directory . run tests/run_test_phases.py --all --skip-offline

# Unit tests only
uv --directory . run tests/run_test_phases.py --unit

# Help
uv --directory . run tests/run_test_phases.py --help
```

## Troubleshooting

**Backend offline?** Use `--skip-offline` flag to skip phases that require backend.

**Import errors?** Ensure you're using `uv --directory .` to run tests.

**Tests timeout?** Check backend is running and accessible on default port (30069).

For detailed information, see `docs/TESTING_PLAN.md`.
