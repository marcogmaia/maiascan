// Copyright (c) Maia

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <fmt/color.h>
#include <fmt/core.h>
#include <CLI/CLI.hpp>

struct Player {
  int32_t health = 100;
  int32_t mana = 100;
  uint8_t gold = 0;
  float x = 0.0f;
  float y = 0.0f;
  std::string name = "Maia";
};

struct GameState {
  uint64_t frame_count = 0;
  double game_time = 0.0;
  std::unique_ptr<Player> local_player;
};

namespace {

// Global pointer to simulate static address scan targets
std::unique_ptr<GameState> g_game;

template <typename T>
void Show(const std::string& name, const T& value) {
  fmt::print("{:15}: {:20} - Addr: {}\r\n",
             name,
             value,
             static_cast<const void*>(&value));
}

template <typename T>
void ShowPointer(const std::string& name, const T* ptr) {
  if (!ptr) {
    fmt::print("{:15}: {:20} - Addr: {}\r\n", name, "null", "0x0");
    return;
  }
  void* stack_ptr = &ptr;  // NOLINT
  std::string val_str;
  if constexpr (std::is_scalar_v<T>) {
    val_str = fmt::format("{}", *ptr);
  } else if constexpr (std::is_same_v<T, std::string>) {
    val_str = fmt::format("\"{}\"", *ptr);
  } else {
    val_str = "...";
  }

  std::string addr_val =
      fmt::format("{}-({})", reinterpret_cast<const void*>(ptr), val_str);
  fmt::print("{:15}: {:20} - Addr: {}\r\n", name, addr_val, stack_ptr);
}

std::atomic_flag global_should_close = ATOMIC_FLAG_INIT;

void SignalHandler(int /*signal*/) {
  global_should_close.test_and_set();
}

}  // namespace

int main(int argc, char** argv) {
  CLI::App app{"FakeGame - A target for MaiaScan memory scanning"};

  bool automatic = false;
  uint32_t interval_ms = 1000;
  int32_t steps = -1;

  app.add_flag("-a,--auto", automatic, "Run automatically without user input");
  app.add_option(
         "-i,--interval", interval_ms, "Update interval in milliseconds")
      ->default_val(1000);
  app.add_option("-s,--steps",
                 steps,
                 "Number of steps to run before exiting (-1 for infinite)")
      ->default_val(-1);

  CLI11_PARSE(app, argc, argv);

  if (std::signal(SIGINT, SignalHandler) == SIG_ERR) {
    fmt::print(
        stderr, fg(fmt::color::red), "Failed to install signal handler\n");
    return 1;
  }

  g_game = std::make_unique<GameState>();
  g_game->local_player = std::make_unique<Player>();

  fmt::print(fg(fmt::color::cyan),
             "FakeGame started. Press Ctrl+C to exit.\r\n");
  fmt::print("GameState Addr: {}\r\n", static_cast<const void*>(g_game.get()));

  int32_t current_step = 0;
  while (!global_should_close.test()) {
    if (steps != -1 && current_step >= steps) {
      break;
    }

    Player* p = g_game->local_player.get();

    fmt::print(fg(fmt::color::yellow), "--- Step {} ---\r\n", current_step);
    Show("Frame", g_game->frame_count);
    Show("Time", g_game->game_time);
    Show("Health", p->health);
    Show("Mana", p->mana);
    Show("Gold", static_cast<int>(p->gold));
    Show("X", p->x);
    Show("Y", p->y);
    Show("Name", p->name);

    // Pointer chains
    ShowPointer("g_game", g_game.get());
    ShowPointer("g_game->player", p);
    ShowPointer("p->health", &p->health);

    if (automatic) {
      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    } else {
      fmt::print("Press Enter to advance step...\r\n");
      std::cin.get();
    }

    // Update state
    g_game->frame_count++;
    g_game->game_time += automatic ? (interval_ms / 1000.0) : 1.0;
    p->health = std::max(0, p->health - 1);
    p->mana -= 2;
    p->gold += 5;
    p->x += 0.1f;
    p->y += 0.2f;
    if (current_step % 5 == 0) {
      p->name += "!";
    }

    current_step++;

    if (p->health <= 0) {
      fmt::print(fg(fmt::color::red), "Player died!\r\n");
      break;
    }
  }

  fmt::print(fg(fmt::color::cyan), "Exiting cleanly...\r\n");
  return 0;
}
