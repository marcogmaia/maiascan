#include <bit>
#include <iostream>
#include <memory>
#include <string>

#include <fmt/core.h>

template <typename T>
void Show(const std::string& name, const T& value) {
  std::cout << fmt::format("{:10}: {:20} - Addr: {}\n", name, value, static_cast<const void*>(&value));
}

// void Show(const std::string& name, const void* value) {
//   std::cout << fmt::format("{:10}: {:20} - Addr: {}\n", name, value, value);
// }

template <typename T>
void ShowPointer(const std::string& name, const T* ptr) {
  void* stack_ptr = &ptr;
  std::string addr_val = fmt::format("{}-({})", std::bit_cast<const void*>(ptr), *ptr);
  std::cout << fmt::format("{:10}: {:20} - Addr: {}\n", name, addr_val, stack_ptr);
}

int main() {
  int health = 100;
  int mana = 100;
  uint8_t gold{};
  int16_t miles = 5;
  float rate = 0.1f;
  double science = 0.1;
  std::string message = "hello world";
  auto leet = std::make_unique<int32_t>(1337);

  while (health > 0) {
    Show("health", health);
    Show("mana", mana);
    Show("gold", gold);
    Show("miles", miles);
    Show("rate", rate);
    Show("science", science);
    Show("message", message);
    ShowPointer("leet", leet.get());

    std::cin.get();

    --health;
    mana -= 2;
    ++gold;
    miles += 10;
    rate += .2f;
    science += .003f;
    message += "!";
    *leet += 3;
  }

  return 0;
}