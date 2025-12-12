"""Run all tests for UnrealMCPProxy."""

import asyncio
import sys
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent / "src"))

from tests.test_proxy_basic import run_all_tests as run_basic_tests
from tests.test_server_integration import run_integration_tests as run_integration_tests


async def main():
    """Run all test suites."""
    print("Running UnrealMCPProxy Test Suite")
    print("=" * 60)
    
    # Run basic tests
    basic_success = await run_basic_tests()
    
    print("\n")
    
    # Run integration tests
    integration_success = await run_integration_tests()
    
    # Final summary
    print("\n" + "=" * 60)
    print("Final Test Results")
    print("=" * 60)
    print(f"Basic Tests: {'✓ PASS' if basic_success else '✗ FAIL'}")
    print(f"Integration Tests: {'✓ PASS' if integration_success else '✗ FAIL'}")
    
    overall_success = basic_success and integration_success
    print(f"\nOverall: {'✓ ALL TESTS PASSED' if overall_success else '✗ SOME TESTS FAILED'}")
    
    return overall_success


if __name__ == "__main__":
    success = asyncio.run(main())
    sys.exit(0 if success else 1)

