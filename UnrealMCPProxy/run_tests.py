"""Run all tests for UnrealMCPProxy.

This script runs the consolidated test suite from test_proxy.py.
All tests have been consolidated into a single, organized test suite.
"""

import asyncio
import sys
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent / "src"))

# Import the consolidated test suite
from tests.test_integration import run_all_tests


async def main():
    """Run the consolidated test suite."""
    print("Running UnrealMCPProxy Consolidated Test Suite")
    print("=" * 60)
    print("\nNote: This test suite consolidates all tests from:")
    print("  - tests/test_integration.py (main integration test suite)")
    print("  - tests/test_unit.py (unit tests with mocks)")
    print("  - tests/test_proxy_basic.py (deprecated)")
    print("  - tests/test_server_integration.py (deprecated)")
    print("\nAll tests are now organized by category in tests/test_integration.py\n")
    
    # Run consolidated tests
    success = await run_all_tests()
    
    return success


if __name__ == "__main__":
    success = asyncio.run(main())
    sys.exit(0 if success else 1)

