# Copyright (c) Maia

import pytest

import maiascan
from conftest import FakeGame


def test_pointer_scan_health(process, fake_game):
    """
    Tests scanning for pointer paths to the health variable.
    Expected path: [game.exe+Offset] -> [Offset] -> Health
    Specifically: g_game -> local_player (16) -> health (0)
    """
    # 1. Get the current target address (Health)
    health_addr = fake_game.get_address(process, "health")

    # 2. Generate Pointer Map
    # This might take a moment, but fakegame is small.
    pmap = maiascan.PointerMap.Generate(process)
    assert pmap.GetEntryCount() > 0, "Pointer map generation returned empty map"

    # 3. Configure Pointer Scan
    config = maiascan.PointerScanConfig()
    config.target_address = health_addr
    config.max_level = 3
    config.max_offset = 4096
    config.max_results = 0  # Unlimited results to ensure we find the right one
    # Filter to paths ending in [..., 16, 0] (the known pointer chain)
    # Natural order: [16, 0] means "offset 16, then offset 0 to reach target"
    config.last_offsets = [16, 0]

    # 4. Run Scan
    scanner = maiascan.PointerScanner()
    modules = process.GetModules()

    # Use the Python binding signature we defined
    result = scanner.FindPaths(pmap, config, modules)

    assert result.success, f"Pointer scan failed: {result.error_message}"
    assert len(result.paths) > 0, "No pointer paths found"

    # Print found paths for debugging
    print(f"\nFound {len(result.paths)} paths:")
    for i, path in enumerate(result.paths):
        print(f"Path {i}: {path}")

    # 5. Validate that we found the known path:
    # g_game is a static pointer to GameState.
    # GameState + 16 points to Player.
    # Player + 0 is Health.
    # So we expect a path ending in offsets [16, 0].

    found_expected = False
    for path in result.paths:
        # path.offsets contains the offsets applied at each level.
        # Check for the pattern [..., 16, 0]
        # Note: offsets are stored from Base -> Target.
        # So [16, 0] means Base -> (Offset 16) -> (Offset 0) -> Target.
        if len(path.offsets) >= 2:
            if path.offsets[-1] == 0 and path.offsets[-2] == 16:
                found_expected = True
                print(f"FOUND EXPECTED PATH: {path}")
                break

    assert found_expected, "Did not find expected pointer path ending in [..., 16, 0]"


def test_pointer_filter(process, fake_game):
    """
    Tests filtering pointer paths.
    We simulate a 'restart' of the game to see if the filter narrows down results.
    """
    # 1. Initial scan in first game instance
    health_addr1 = fake_game.get_address(process, "health")
    pmap1 = maiascan.PointerMap.Generate(process)

    config = maiascan.PointerScanConfig()
    config.target_address = health_addr1
    config.max_level = 2
    config.max_offset = 1024

    scanner = maiascan.PointerScanner()
    modules1 = process.GetModules()
    result1 = scanner.FindPaths(pmap1, config, modules1)

    assert result1.success
    initial_count = len(result1.paths)
    print(f"\nInitial paths found: {initial_count}")

    # 2. Start a SECOND game instance
    fake_game2 = FakeGame()
    try:
        process2 = maiascan.Process.Create(fake_game2.pid)
        health_addr2 = fake_game2.get_address(process2, "health")

        # 3. Filter the paths from game 1 using game 2's state
        # FilterPaths takes the list of paths and the new target address
        filtered_paths = scanner.FilterPaths(process2, result1.paths, health_addr2)

        filtered_count = len(filtered_paths)
        print(f"Filtered paths: {filtered_count}")

        # The correct path [16, 0] should still be there
        found_expected = False
        for path in filtered_paths:
            if (
                len(path.offsets) >= 2
                and path.offsets[-1] == 0
                and path.offsets[-2] == 16
            ):
                found_expected = True
                break

        assert found_expected, "Expected path lost during filtering"
        # Usually, filtering should reduce the count if there were false positives
        # but even if it doesn't (fakegame is very clean), we verified it doesn't crash and preserves the right path.
        assert filtered_count <= initial_count

    finally:
        fake_game2.stop()


if __name__ == "__main__":
    import sys
    import pytest

    sys.exit(pytest.main([__file__]))
