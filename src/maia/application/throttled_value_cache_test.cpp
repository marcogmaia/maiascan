// Copyright (c) Maia

#include "maia/application/throttled_value_cache.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

namespace maia {
namespace {

using TimePoint = ThrottledValueCache::TimePoint;
using Duration = ThrottledValueCache::Duration;
using FetchResult = std::optional<std::vector<std::byte>>;

std::vector<std::byte> MakeBytes(std::initializer_list<uint8_t> values) {
  std::vector<std::byte> result;
  result.reserve(values.size());
  for (uint8_t v : values) {
    result.push_back(static_cast<std::byte>(v));
  }
  return result;
}

class ThrottledValueCacheTest : public ::testing::Test {
 protected:
  testing::MockFunction<FetchResult(uint64_t)> mock_fetcher_;
  ThrottledValueCache cache_{std::chrono::milliseconds(100)};
};

TEST_F(ThrottledValueCacheTest, CacheMissCallsFetch) {
  TimePoint now{};

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(MakeBytes({0x42, 0x43})));

  auto result = cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), now);

  EXPECT_TRUE(result);
  EXPECT_EQ(result->size(), 2);
}

TEST_F(ThrottledValueCacheTest, CacheHitReturnsCachedValue) {
  TimePoint now{};

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(MakeBytes({0x42})));

  ASSERT_TRUE(cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), now));

  testing::MockFunction<FetchResult(uint64_t)> second_fetcher;
  EXPECT_CALL(second_fetcher, Call(testing::_)).Times(0);

  auto result = cache_.Get(0x1000, second_fetcher.AsStdFunction(), now);

  EXPECT_TRUE(result);
  EXPECT_EQ((*result)[0], std::byte{0x42});
}

TEST_F(ThrottledValueCacheTest, CacheExpiresAfterDuration) {
  TimePoint now{};
  Duration duration = std::chrono::milliseconds(100);

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(MakeBytes({0x42})));

  ASSERT_TRUE(cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), now));

  TimePoint before_expiry = now + std::chrono::milliseconds(99);

  testing::MockFunction<FetchResult(uint64_t)> before_expiry_fetcher;
  EXPECT_CALL(before_expiry_fetcher, Call(testing::_)).Times(0);

  ASSERT_TRUE(
      cache_.Get(0x1000, before_expiry_fetcher.AsStdFunction(), before_expiry));

  TimePoint at_expiry = now + duration;

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(MakeBytes({0xFF})));

  ASSERT_TRUE(cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), at_expiry));
}

TEST_F(ThrottledValueCacheTest, DifferentAddressesCachedSeparately) {
  TimePoint now{};

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(MakeBytes({0x00})));

  auto result1 = cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), now);
  EXPECT_EQ((*result1)[0], std::byte{0x00});

  EXPECT_CALL(mock_fetcher_, Call(0x2000))
      .WillOnce(testing::Return(MakeBytes({0x00})));

  auto result2 = cache_.Get(0x2000, mock_fetcher_.AsStdFunction(), now);
  EXPECT_EQ((*result2)[0], std::byte{0x00});

  EXPECT_EQ(cache_.Size(), 2);

  testing::MockFunction<FetchResult(uint64_t)> fetcher_1000;
  testing::MockFunction<FetchResult(uint64_t)> fetcher_2000;
  EXPECT_CALL(fetcher_1000, Call(testing::_)).Times(0);
  EXPECT_CALL(fetcher_2000, Call(testing::_)).Times(0);

  ASSERT_TRUE(cache_.Get(0x1000, fetcher_1000.AsStdFunction(), now));
  ASSERT_TRUE(cache_.Get(0x2000, fetcher_2000.AsStdFunction(), now));
}

TEST_F(ThrottledValueCacheTest, FetchFailureReturnsNullopt) {
  TimePoint now{};

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(std::nullopt));

  auto result = cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), now);

  EXPECT_FALSE(result);
  EXPECT_EQ(cache_.Size(), 0);
}

TEST_F(ThrottledValueCacheTest, ClearRemovesAllEntries) {
  TimePoint now{};

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(MakeBytes({0x42})));
  EXPECT_CALL(mock_fetcher_, Call(0x2000))
      .WillOnce(testing::Return(MakeBytes({0x43})));

  ASSERT_TRUE(cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), now));
  ASSERT_TRUE(cache_.Get(0x2000, mock_fetcher_.AsStdFunction(), now));

  EXPECT_EQ(cache_.Size(), 2);

  cache_.Clear();

  EXPECT_EQ(cache_.Size(), 0);

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(MakeBytes({0x42})));

  ASSERT_TRUE(cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), now));
}

TEST_F(ThrottledValueCacheTest, ExpiredEntryReplacedWithNewValue) {
  TimePoint now{};

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(MakeBytes({0x42})));

  ASSERT_TRUE(cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), now));

  TimePoint later = now + std::chrono::milliseconds(150);

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(MakeBytes({0x99})));

  auto result = cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), later);

  EXPECT_EQ((*result)[0], std::byte{0x99});
}

TEST_F(ThrottledValueCacheTest, PartialExpiryOnlyExpiredReFetched) {
  TimePoint now{};

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(MakeBytes({0x11})));

  ASSERT_TRUE(cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), now));

  TimePoint later = now + std::chrono::milliseconds(50);

  EXPECT_CALL(mock_fetcher_, Call(0x2000))
      .WillOnce(testing::Return(MakeBytes({0x22})));

  ASSERT_TRUE(cache_.Get(0x2000, mock_fetcher_.AsStdFunction(), later));

  TimePoint check_time = now + std::chrono::milliseconds(120);

  EXPECT_CALL(mock_fetcher_, Call(0x1000))
      .WillOnce(testing::Return(MakeBytes({0xAA})));

  auto result1 = cache_.Get(0x1000, mock_fetcher_.AsStdFunction(), check_time);
  EXPECT_EQ((*result1)[0], std::byte{0xAA});

  testing::MockFunction<FetchResult(uint64_t)> fetcher_2000;
  EXPECT_CALL(fetcher_2000, Call(testing::_)).Times(0);

  auto result2 = cache_.Get(0x2000, fetcher_2000.AsStdFunction(), check_time);
  EXPECT_EQ((*result2)[0], std::byte{0x22});
}

}  // namespace
}  // namespace maia
