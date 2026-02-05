# Copyright (c) Maia

import struct
import pytest
import maiascan


def test_initial_unknown_scan(scanner, process, fake_game):
    """Tests scanning with Initial Unknown Value followed by Changed."""
    # 1. First Scan: Unknown Initial Value (Int32)
    # This captures a snapshot of memory.
    config = maiascan.ScanConfig()
    config.value_type = maiascan.ScanValueType.kInt32
    config.comparison = maiascan.ScanComparison.kUnknown
    config.alignment = 4
    # value is ignored for kUnknown, but good practice to clear it
    config.value = b""

    result = scanner.FirstScan(process, config)
    assert result.success, f"First scan failed: {result.error_message}"
    assert len(result.storage.addresses) > 0

    # 2. Step the game
    fake_game.step()

    # 3. Next Scan: Changed value
    config.comparison = maiascan.ScanComparison.kChanged
    config.use_previous_results = True

    next_result = scanner.NextScan(process, config, result.storage)
    assert next_result.success, f"Next scan failed: {next_result.error_message}"

    # Verify Health (which changes every frame) is in the results
    # Health decreases by 1, so it counts as Changed.
    health_addr = fake_game.get_address(process, "health")
    assert health_addr in next_result.storage.addresses


def test_increased_scan(scanner, process, fake_game):
    """Tests scanning for Increased value (Gold increases)."""
    # 1. First Scan: Gold = 0 (UInt8)
    config = maiascan.ScanConfig()
    config.value_type = maiascan.ScanValueType.kUInt8
    config.comparison = maiascan.ScanComparison.kExactValue
    config.value = struct.pack("<B", 0)
    config.alignment = 1

    result = scanner.FirstScan(process, config)
    assert result.success, f"First scan failed: {result.error_message}"

    # 2. Step the game (Gold increases by 5)
    fake_game.step()

    # 3. Next Scan: Increased
    config.comparison = maiascan.ScanComparison.kIncreased
    config.value = b""  # Not needed for simple Increased check
    config.use_previous_results = True

    next_result = scanner.NextScan(process, config, result.storage)
    assert next_result.success, f"Next scan failed: {next_result.error_message}"

    # Verify Gold address is in results
    gold_addr = fake_game.get_address(process, "gold")
    assert gold_addr in next_result.storage.addresses


def test_increased_by_scan(scanner, process, fake_game):
    """Tests scanning for Increased By specific value (Gold +5)."""
    # 1. First Scan: Gold = 0 (UInt8)
    config = maiascan.ScanConfig()
    config.value_type = maiascan.ScanValueType.kUInt8
    config.comparison = maiascan.ScanComparison.kExactValue
    config.value = struct.pack("<B", 0)
    config.alignment = 1

    result = scanner.FirstScan(process, config)
    assert result.success, f"First scan failed: {result.error_message}"

    # 2. Step the game (Gold increases by 5)
    fake_game.step()

    # 3. Next Scan: Increased By 5
    config.comparison = maiascan.ScanComparison.kIncreasedBy
    config.value = struct.pack("<B", 5)
    config.use_previous_results = True

    next_result = scanner.NextScan(process, config, result.storage)
    assert next_result.success, f"Next scan failed: {next_result.error_message}"

    # Verify Gold address is in results
    gold_addr = fake_game.get_address(process, "gold")
    assert gold_addr in next_result.storage.addresses


def test_decreased_by_scan(scanner, process, fake_game):
    """Tests scanning for Decreased By specific value (Mana -2)."""
    # 1. First Scan: Mana = 100 (Int32)
    config = maiascan.ScanConfig()
    config.value_type = maiascan.ScanValueType.kInt32
    config.comparison = maiascan.ScanComparison.kExactValue
    config.value = struct.pack("<i", 100)
    config.alignment = 4

    result = scanner.FirstScan(process, config)
    assert result.success, f"First scan failed: {result.error_message}"

    # 2. Step the game (Mana decreases by 2)
    fake_game.step()

    # 3. Next Scan: Decreased By 2
    config.comparison = maiascan.ScanComparison.kDecreasedBy
    config.value = struct.pack("<i", 2)
    config.use_previous_results = True

    next_result = scanner.NextScan(process, config, result.storage)
    assert next_result.success, f"Next scan failed: {next_result.error_message}"

    # Verify Mana address is in results
    mana_addr = fake_game.get_address(process, "mana")
    assert mana_addr in next_result.storage.addresses


def test_float_scan(scanner, process, fake_game):
    """Tests scanning for Float values (X increases)."""
    # 1. First Scan: X = 0.0 (Float)
    config = maiascan.ScanConfig()
    config.value_type = maiascan.ScanValueType.kFloat
    config.comparison = maiascan.ScanComparison.kExactValue
    config.value = struct.pack("<f", 0.0)
    config.alignment = 4

    result = scanner.FirstScan(process, config)
    assert result.success, f"First scan failed: {result.error_message}"

    # 2. Step the game (X increases by 0.1)
    fake_game.step()

    # 3. Next Scan: Increased
    # Note: Exact floating point matching can be tricky, but Increased should be robust
    config.comparison = maiascan.ScanComparison.kIncreased
    config.value = b""
    config.use_previous_results = True

    next_result = scanner.NextScan(process, config, result.storage)
    assert next_result.success, f"Next scan failed: {next_result.error_message}"

    # Verify X address is in results
    x_addr = fake_game.get_address(process, "x")
    assert x_addr in next_result.storage.addresses


def test_string_scan(scanner, process, fake_game):
    """Tests scanning for String values (Name changes)."""
    # 1. First Scan: Name = "Maia" (String)
    config = maiascan.ScanConfig()
    config.value_type = maiascan.ScanValueType.kString
    config.comparison = maiascan.ScanComparison.kExactValue
    config.value = b"Maia"
    # String scanning doesn't strictly require alignment, but usually 1 is fine
    config.alignment = 1

    result = scanner.FirstScan(process, config)
    assert result.success, f"First scan failed: {result.error_message}"

    # 2. Step the game 5 times (Name becomes "Maia!")
    for _ in range(5):
        fake_game.step()

    # 3. Next Scan: Name = "Maia!"
    # Note: Strings are usually searched as ExactValue of the new string
    config.comparison = maiascan.ScanComparison.kExactValue
    config.value = b"Maia!"
    config.use_previous_results = True

    next_result = scanner.NextScan(process, config, result.storage)
    assert next_result.success, f"Next scan failed: {next_result.error_message}"

    # Verify Name address is in results
    name_addr = fake_game.get_address(process, "name")
    assert name_addr in next_result.storage.addresses


if __name__ == "__main__":
    import sys

    sys.exit(pytest.main([__file__]))
