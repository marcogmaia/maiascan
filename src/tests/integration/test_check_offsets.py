# Copyright (c) Maia

import struct


def test_verify_offsets(fake_game, process):
    """Verifies that the enhanced FakeGame correctly resolves offsets."""
    player_addr = fake_game.get_player_address(process)
    assert player_addr != 0

    expected_offsets = {
        "health": 0,
        "mana": 4,
        "gold": 8,
        "x": 12,
        "y": 16,
        "name": 24,
    }

    for var, offset in expected_offsets.items():
        addr = fake_game.get_address(process, var)
        assert addr == player_addr + offset
        print(f"Verified {var} at offset {offset}")

    # Verify we can read the initial health (100)
    health_addr = fake_game.get_address(process, "health")
    health_data = process.ReadMemory(health_addr, 4)
    assert len(health_data) == 4
    health = struct.unpack("<i", health_data)[0]
    assert health == 100

    # Verify we can read name (should be "Maia")
    name_addr = fake_game.get_address(process, "name")
    name_data = process.ReadMemory(name_addr, 16)
    name = name_data.split(b"\0")[0].decode("ascii")
    assert name == "Maia"
    print(f"Verified player name: {name}")
