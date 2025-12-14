"""Test runner for UnrealMCPProxy testing phases.

This script runs tests organized by phase as defined in docs/TESTING_PLAN.md.
All tests use the `uv` tool to ensure compatibility with production environment.
"""

import asyncio
import sys
import argparse
from pathlib import Path
from typing import List, Tuple

# Add src to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from unreal_mcp_proxy.client.unreal_mcp import UnrealMCPClient, ConnectionState

# Import test functions from test_integration
# Use importlib to handle the module import correctly
import importlib.util
test_integration_path = Path(__file__).parent / "test_integration.py"
spec = importlib.util.spec_from_file_location("test_integration", test_integration_path)
test_integration = importlib.util.module_from_spec(spec)
spec.loader.exec_module(test_integration)

# Phase 1: Offline tests
test_all_tools_defined = test_integration.test_all_tools_defined
test_cached_tool_definitions = test_integration.test_cached_tool_definitions
test_prompt_definitions_file = test_integration.test_prompt_definitions_file
test_resource_definitions_file = test_integration.test_resource_definitions_file
test_error_invalid_tool = test_integration.test_error_invalid_tool
test_error_offline_backend = test_integration.test_error_offline_backend
# Phase 2: Backend connection
test_backend_connection = test_integration.test_backend_connection
test_tool_list = test_integration.test_tool_list
# Phase 3: Tool calls
test_get_project_config = test_integration.test_get_project_config
test_client_direct = test_integration.test_client_direct
# Phase 4: Schema compatibility
test_schema_comparison = test_integration.test_schema_comparison
test_description_matching = test_integration.test_description_matching
# Phase 5: Error paths
test_edge_case_empty_arguments = test_integration.test_edge_case_empty_arguments
# Phase 6: Resources
test_test_assets_exist = test_integration.test_test_assets_exist
test_resources_list = test_integration.test_resources_list
test_resource_templates_list = test_integration.test_resource_templates_list
test_read_resource = test_integration.test_read_resource
# Phase 7: Prompts
test_prompts_list = test_integration.test_prompts_list
test_get_prompt = test_integration.test_get_prompt


# Phase definitions
PHASES = {
    "1": {
        "name": "Phase 1: Offline Validation",
        "description": "Tests that work without backend connection",
        "tests": [
            ("All Tools Defined", test_all_tools_defined),
            ("Cached Tool Definitions", test_cached_tool_definitions),
            ("Prompt Definitions File", test_prompt_definitions_file),
            ("Resource Definitions File", test_resource_definitions_file),
            ("Error Handling - Invalid Tool", test_error_invalid_tool),
            ("Error Handling - Offline Backend", test_error_offline_backend),
        ],
        "requires_backend": False,
    },
    "2": {
        "name": "Phase 2: Backend Connection",
        "description": "Tests requiring backend connection",
        "tests": [
            ("Backend Connection", test_backend_connection),
            ("Tools List", test_tool_list),
        ],
        "requires_backend": True,
    },
    "3": {
        "name": "Phase 3: Tool Calls",
        "description": "Tests for tool call functionality",
        "tests": [
            ("get_project_config", test_get_project_config),
            ("Client Direct", test_client_direct),
        ],
        "requires_backend": True,
    },
    "4": {
        "name": "Phase 4: Schema Compatibility",
        "description": "Tests for tool definition compatibility",
        "tests": [
            ("Schema Comparison", test_schema_comparison),
            ("Description Matching", test_description_matching),
        ],
        "requires_backend": True,
    },
    "5": {
        "name": "Phase 5: Error Paths and Edge Cases",
        "description": "Tests for error handling and edge cases",
        "tests": [
            ("Edge Case - Empty Tool Arguments", test_edge_case_empty_arguments),
        ],
        "requires_backend": True,
    },
    "6": {
        "name": "Phase 6: Resources",
        "description": "Tests for resource and resource template functionality",
        "tests": [
            ("Test Assets Exist", test_test_assets_exist),
            ("Resources List", test_resources_list),
            ("Resource Templates List", test_resource_templates_list),
            ("Read Resource", test_read_resource),
        ],
        "requires_backend": True,
    },
    "7": {
        "name": "Phase 7: Prompts",
        "description": "Tests for prompt functionality",
        "tests": [
            ("Prompts List", test_prompts_list),
            ("Get Prompt", test_get_prompt),
        ],
        "requires_backend": True,
    },
}


async def check_backend_online() -> bool:
    """Check if backend is online by making an actual connection test."""
    client = UnrealMCPClient()
    try:
        # Initialize the client (performs immediate connection check)
        await client.initialize_async()
        
        # After initialization, state should be set immediately
        # But also verify with an actual call to be sure
        if client.state == ConnectionState.ONLINE:
            return True
        
        # If state is still UNKNOWN or OFFLINE, try a quick test
        try:
            response = await client.call_method("tools/list", {})
            # If we get a response (even with error), the backend is reachable
            return True
        except (ConnectionError, TimeoutError):
            return False
        except Exception:
            # Any other exception might indicate backend is online but had an issue
            return False
    finally:
        await client.close()


async def run_phase(phase_num: str, skip_if_offline: bool = False) -> Tuple[bool, List[str]]:
    """Run a specific phase of tests.
    
    Args:
        phase_num: Phase number (1-5)
        skip_if_offline: If True, skip phase if backend is offline (instead of failing)
    
    Returns:
        Tuple of (success, list of messages)
    """
    if phase_num not in PHASES:
        return False, [f"Unknown phase: {phase_num}"]
    
    phase = PHASES[phase_num]
    messages = []
    
    print("\n" + "=" * 60)
    print(phase["name"])
    print("=" * 60)
    print(phase["description"])
    print()
    
    # Check backend requirement
    if phase["requires_backend"]:
        backend_online = await check_backend_online()
        if not backend_online:
            if skip_if_offline:
                messages.append(f"[SKIP] Phase {phase_num} requires backend (offline)")
                print(f"  [SKIP] Backend is offline - skipping Phase {phase_num}")
                print("         Start Unreal Editor with UnrealMCPServer plugin enabled to run this phase")
                return True, messages
            else:
                messages.append(f"[FAIL] Phase {phase_num} requires backend (offline)")
                print(f"  [FAIL] Backend is offline - Phase {phase_num} cannot run")
                return False, messages
        else:
            messages.append(f"[INFO] Backend is online - running Phase {phase_num}")
    
    # Run tests
    results: List[Tuple[str, bool]] = []
    for test_name, test_func in phase["tests"]:
        try:
            result = await test_func()
            results.append((test_name, result))
            status = "[OK] PASS" if result else "[X] FAIL"
            messages.append(f"{status}: {test_name}")
        except Exception as e:
            results.append((test_name, False))
            messages.append(f"[X] FAIL: {test_name} - Exception: {str(e)}")
            import traceback
            traceback.print_exc()
    
    # Summary
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    print(f"\n  Phase {phase_num} Summary: {passed}/{total} tests passed")
    
    success = passed == total
    return success, messages


async def run_all_phases(skip_offline_phases: bool = False) -> bool:
    """Run all phases in sequence.
    
    Args:
        skip_offline_phases: If True, skip phases that require backend when offline
    
    Returns:
        True if all phases passed
    """
    print("=" * 60)
    print("UnrealMCPProxy Phased Testing Plan")
    print("=" * 60)
    print("\nRunning all phases in sequence...")
    print("See docs/TESTING_PLAN.md for detailed information about each phase.\n")
    
    all_messages = []
    all_success = True
    
    for phase_num in sorted(PHASES.keys()):
        success, messages = await run_phase(phase_num, skip_if_offline=skip_offline_phases)
        all_messages.extend(messages)
        if not success:
            all_success = False
    
    # Final summary
    print("\n" + "=" * 60)
    print("Final Summary")
    print("=" * 60)
    
    for message in all_messages:
        print(f"  {message}")
    
    print("\n" + "=" * 60)
    if all_success:
        print("[OK] All phases completed successfully")
    else:
        print("[FAIL] Some phases failed - see details above")
    print("=" * 60)
    
    return all_success


async def run_unit_tests() -> bool:
    """Run unit tests (Phase 2 equivalent, but separate)."""
    print("\n" + "=" * 60)
    print("Unit Tests (with Mocks)")
    print("=" * 60)
    print("Running unit tests from tests/test_unit.py...")
    print()
    
    # Import and run unit tests
    try:
        import subprocess
        # Use uv to run pytest to ensure correct environment
        # Note: pytest is in optional dev dependencies
        # Get the project root (parent of tests directory)
        project_root = Path(__file__).parent.parent
        result = subprocess.run(
            ["uv", "--directory", str(project_root), "run", "python", "-m", "pytest", "tests/test_unit.py", "-v"],
            cwd=project_root,
            capture_output=False
        )
        return result.returncode == 0
    except FileNotFoundError:
        print("  [W] pytest not found. Install dev dependencies:")
        print("      uv pip install -e '.[dev]'")
        return False
    except Exception as e:
        print(f"  [X] Error running unit tests: {str(e)}")
        print(f"  [INFO] Try running manually: uv --directory . run python -m pytest tests/test_unit.py -v")
        return False


def main():
    """Main entry point for test runner."""
    parser = argparse.ArgumentParser(
        description="Run UnrealMCPProxy tests by phase",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run all phases (skip offline phases if backend unavailable)
  python run_test_phases.py --all --skip-offline

  # Run specific phase
  python run_test_phases.py --phase 1

  # Run unit tests only
  python run_test_phases.py --unit

  # Run phases 1-3
  python run_test_phases.py --phases 1 2 3
        """
    )
    
    parser.add_argument(
        "--phase",
        type=str,
        help="Run a specific phase (1-5)"
    )
    
    parser.add_argument(
        "--phases",
        nargs="+",
        help="Run multiple phases (e.g., --phases 1 2 3)"
    )
    
    parser.add_argument(
        "--all",
        action="store_true",
        help="Run all phases"
    )
    
    parser.add_argument(
        "--unit",
        action="store_true",
        help="Run unit tests only"
    )
    
    parser.add_argument(
        "--skip-offline",
        action="store_true",
        help="Skip phases that require backend when backend is offline"
    )
    
    args = parser.parse_args()
    
    # Determine what to run
    if args.unit:
        success = asyncio.run(run_unit_tests())
        sys.exit(0 if success else 1)
    
    if args.phase:
        success, messages = asyncio.run(run_phase(args.phase, skip_if_offline=args.skip_offline))
        for message in messages:
            print(f"  {message}")
        sys.exit(0 if success else 1)
    
    if args.phases:
        all_success = True
        for phase_num in args.phases:
            success, messages = asyncio.run(run_phase(phase_num, skip_if_offline=args.skip_offline))
            all_success = all_success and success
            for message in messages:
                print(f"  {message}")
        sys.exit(0 if all_success else 1)
    
    if args.all:
        success = asyncio.run(run_all_phases(skip_offline_phases=args.skip_offline))
        sys.exit(0 if success else 1)
    
    # Default: show help
    parser.print_help()
    sys.exit(1)


if __name__ == "__main__":
    main()

