import os
import subprocess
import re
import time
import struct
import pytest
import maiascan


class FakeGame:
    """
    Manages the fakegame.exe process for integration testing.
    Captures the GameState address from stdout and allows stepping the game logic.
    """

    def __init__(self, executable_path=None):
        if executable_path is None:
            # Common build output locations for the project
            possible_paths = [
                "out/build/x64-windows/src/fakegame/fakegame.exe",
                "out/build/windows-release/src/fakegame/fakegame.exe",
                "out/build/windows-debug/src/fakegame/fakegame.exe",
                "bin/fakegame.exe",
                "fakegame.exe",
            ]
            for p in possible_paths:
                if os.path.exists(p):
                    executable_path = p
                    break

        # If still not found, assume it's in the PATH or will be provided by environment
        self.executable = executable_path or "fakegame.exe"

        # We use a robust subprocess configuration to handle the interactive target
        self.process = subprocess.Popen(
            [self.executable],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout for easier parsing
            text=True,
            bufsize=1,  # Line buffered to avoid blocking on address discovery
        )

        self.game_state_addr = None
        self.pid = self.process.pid

        # Parse stdout for GameState address
        # The fake game outputs "GameState Addr: 0x..." on startup
        start_time = time.time()
        while time.time() - start_time < 5:
            if self.process.stdout is None:
                break
            line = self.process.stdout.readline()
            if not line:
                break

            # Match "GameState Addr: 0x7ff7..."
            match = re.search(r"GameState Addr:\s+(0x[0-9a-fA-F]+)", line)
            if match:
                self.game_state_addr = int(match.group(1), 16)
                break

        if self.game_state_addr is None:
            self.stop()
            raise RuntimeError(
                f"Failed to capture GameState address from {self.executable}"
            )

    def get_player_address(self, process):
        """Resolves the local_player pointer from GameState."""
        if self.game_state_addr is None:
            raise RuntimeError("GameState address not captured")

        # GameState structure (from fakegame/main.cpp):
        #   offset 0: frame_count (8 bytes)
        #   offset 8: game_time (8 bytes)
        #   offset 16: local_player pointer (8 bytes)
        player_ptr_addr = self.game_state_addr + 16
        ptr_data = process.ReadMemory(player_ptr_addr, 8)

        if not ptr_data or len(ptr_data) != 8:
            raise RuntimeError("Failed to read local_player pointer")

        # Unpack as uint64_t (little endian)
        return struct.unpack("<Q", ptr_data)[0]

    def get_address(self, process, variable_name):
        """Returns the absolute memory address of a player attribute."""
        player_addr = self.get_player_address(process)
        offsets = {
            "health": 0,
            "mana": 4,
            "gold": 8,
            "x": 12,
            "y": 16,
            "name": 24,
        }

        if variable_name.lower() not in offsets:
            raise ValueError(f"Unknown variable name: {variable_name}")

        return player_addr + offsets[variable_name.lower()]

    def step(self):
        """Advances the game state by sending a newline to stdin."""
        if self.process and self.process.poll() is None and self.process.stdin:
            self.process.stdin.write("\n")
            self.process.stdin.flush()
            # Give the target process a moment to update memory and sync with the OS
            time.sleep(0.1)

    def stop(self):
        """Gracefully terminates the fake game process."""
        if self.process:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                # Force kill if it doesn't shut down within the grace period
                self.process.kill()
            self.process = None

    def __del__(self):
        # Ensure cleanup even if stop() wasn't called explicitly
        self.stop()


@pytest.fixture
def fake_game():
    game = FakeGame()
    yield game
    game.stop()


@pytest.fixture
def process(fake_game):
    return maiascan.Process.Create(fake_game.pid)


@pytest.fixture
def scanner():
    return maiascan.Scanner()
