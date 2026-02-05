# Copyright (c) Maia

import sys
import struct
import pytest
import maiascan


def test_scan_health(scanner, process, fake_game):
    """Tests scanning for a known exact health value and following it after change."""
    # 1. First Scan for Health = 100 (initial value)
    config = maiascan.ScanConfig()
    config.value_type = maiascan.ScanValueType.kInt32
    config.comparison = maiascan.ScanComparison.kExactValue
    config.value = struct.pack("<i", 100)
    config.alignment = 4

    result = scanner.FirstScan(process, config)
    assert result.success, f"First scan failed: {result.error_message}"
    assert len(result.storage.addresses) > 0

    # 2. Step the game (Health decreases by 1 each step)
    fake_game.step()  # Health is now 99

    # 3. Next Scan for Health = 99
    config.value = struct.pack("<i", 99)
    config.use_previous_results = True

    next_result = scanner.NextScan(process, config, result.storage)
    assert next_result.success, f"Next scan failed: {next_result.error_message}"

    # Verify that the actual health address is among the results
    health_addr = fake_game.get_address(process, "health")
    assert health_addr in next_result.storage.addresses


def test_scan_decreased(scanner, process, fake_game):
    """Tests scanning for a decreased value after several steps."""
    # 1. First Scan for Health in a range [90, 110]
    config = maiascan.ScanConfig()
    config.value_type = maiascan.ScanValueType.kInt32
    config.comparison = maiascan.ScanComparison.kBetween
    # 'Between' comparison expects two values: value (start) and value_end (end)
    config.value = struct.pack("<i", 90)
    config.value_end = struct.pack("<i", 110)

    config.alignment = 4

    result = scanner.FirstScan(process, config)
    assert result.success, f"First scan failed: {result.error_message}"

    # 2. Step the game several times to decrease health
    fake_game.step()
    fake_game.step()

    # 3. Next Scan for Decreased
    config.comparison = maiascan.ScanComparison.kDecreased
    config.value = b""  # No explicit value needed for relative comparison
    config.use_previous_results = True

    next_result = scanner.NextScan(process, config, result.storage)
    assert next_result.success, f"Next scan failed: {next_result.error_message}"

    # Verify that the health address is still in the result set
    health_addr = fake_game.get_address(process, "health")
    assert health_addr in next_result.storage.addresses


if __name__ == "__main__":
    import sys
    import pytest

    sys.exit(pytest.main([__file__]))
