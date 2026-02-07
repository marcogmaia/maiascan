#include "maia/core/address_formatter.h"
#include <gtest/gtest.h>

namespace maia {

TEST(AddressFormatterTest, FormatsAbsoluteAddressWhenNoModules) {
  AddressFormatter formatter({});
  auto result = formatter.Format(0x12345678);
  EXPECT_EQ(result.text, "0x12345678");
  EXPECT_FALSE(result.is_relative);
}

TEST(AddressFormatterTest, FormatsRelativeAddressWithinModule) {
  std::vector<mmem::ModuleDescriptor> modules = {
      {.base = 0x1000,
       .end = 0x2000,
       .size = 0x1000,
       .path = "C:\\test.exe",
       .name = "test.exe"}
  };
  AddressFormatter formatter(modules);

  auto result = formatter.Format(0x1500);
  EXPECT_EQ(result.text, "test.exe+0x500");
  EXPECT_TRUE(result.is_relative);
}

TEST(AddressFormatterTest, FormatsAbsoluteAddressOutsideModule) {
  std::vector<mmem::ModuleDescriptor> modules = {
      {.base = 0x1000,
       .end = 0x2000,
       .size = 0x1000,
       .path = "C:\\test.exe",
       .name = "test.exe"}
  };
  AddressFormatter formatter(modules);

  auto result = formatter.Format(0x500);
  EXPECT_EQ(result.text, "0x500");
  EXPECT_FALSE(result.is_relative);

  result = formatter.Format(0x2500);
  EXPECT_EQ(result.text, "0x2500");
  EXPECT_FALSE(result.is_relative);
}

TEST(AddressFormatterTest, HandlesMultipleModules) {
  std::vector<mmem::ModuleDescriptor> modules = {
      {.base = 0x1000,
       .end = 0x2000,
       .size = 0x1000,
       .path = "C:\\a.dll",
       .name = "a.dll"},
      {.base = 0x3000,
       .end = 0x4000,
       .size = 0x1000,
       .path = "C:\\b.dll",
       .name = "b.dll"}
  };
  AddressFormatter formatter(modules);

  auto result = formatter.Format(0x1100);
  EXPECT_EQ(result.text, "a.dll+0x100");

  result = formatter.Format(0x3100);
  EXPECT_EQ(result.text, "b.dll+0x100");

  result = formatter.Format(0x2500);
  EXPECT_EQ(result.text, "0x2500");
}

}  // namespace maia
