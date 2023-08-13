#include <iostream>
#include <string>

#include <fmt/core.h>

template <typename T>
void Show(const std::string& name, const T& value) {
  std::cout << fmt::format("{:10}: {:20} - Addr: {}\n", name, value, static_cast<const void*>(&value));
}

int main() {
  int health = 100;
  int mana = 100;
  uint8_t gold{};
  int16_t miles = 5;
  float rate = 0.1f;
  double science = 0.1;
  std::string message = "hello world";

  while (health > 0) {
    Show("health", health);
    Show("mana", mana);
    Show("gold", gold);
    Show("miles", miles);
    Show("rate", rate);
    Show("science", science);
    Show("message", message);

    std::cin.get();

    --health;
    mana -= 2;
    ++gold;
    miles += 10;
    rate += .2f;
    science += .003f;
    message += "!";
  }

  return 0;
}
