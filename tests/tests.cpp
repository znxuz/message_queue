#include <gtest/gtest.h>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "../message_queue.hpp"

using namespace message_queue;

using Priority = MessageQueue::Priority;

class MessageQueueTest : public ::testing::Test {
 protected:
  MessageQueue mq = MessageQueue{"/my_queue"};
};

TEST_F(MessageQueueTest, Construction) {
  EXPECT_THROW(MessageQueue{""}, std::invalid_argument);
  EXPECT_THROW(MessageQueue{"/"}, std::invalid_argument);
  EXPECT_THROW(MessageQueue{"no_slash"}, std::invalid_argument);
  EXPECT_THROW(MessageQueue{"/more_than_one_slash/"}, std::invalid_argument);
  EXPECT_THROW(MessageQueue{"/more/_than_one_slash"}, std::invalid_argument);

  EXPECT_TRUE(mq.is_empty());
  EXPECT_EQ(mq.size(), 0);
  EXPECT_EQ(mq.type(), MqType::BIDIRECTIONAL);
  EXPECT_EQ(mq.mode(), MqMode::NON_BLOCKING);
}

TEST_F(MessageQueueTest, EmptyLogError) {
  EXPECT_FALSE(mq.receive());
}

TEST_F(MessageQueueTest, SendLogFull) {
}

TEST_F(MessageQueueTest, SendRawPointer) {
  auto s = "hello";
  EXPECT_TRUE(mq.send(s));
  auto ret = mq.receive();
  EXPECT_TRUE(ret);
  const auto& [msg, prio] = ret.value();
  EXPECT_EQ(s, msg);
}

TEST_F(MessageQueueTest, SendContainer) {
  using namespace std::literals;
  auto round_trip_check = [](MessageQueue& mq, const auto& msg) static {
    EXPECT_TRUE(mq.send(msg));
    auto ret = mq.receive();
    EXPECT_TRUE(ret);
    const auto& [str, prio] = ret.value();
    EXPECT_EQ(prio, Priority::DEFAULT);
    for (size_t i = 0; i < str.size(); ++i) EXPECT_EQ(msg[i], str[i]);
  };

  auto sv = "hello"sv;
  round_trip_check(mq, sv);
  round_trip_check(mq, std::span{sv});
  round_trip_check(mq, std::string{"world"});
  round_trip_check(mq, std::array<char, 3>{'m', 's', 'g'});
  round_trip_check(mq, std::array<uint8_t, 3>{'m', 's', 'g'});
  round_trip_check(mq, std::vector<char>{'m', 's', 'g'});
  round_trip_check(mq, std::vector<uint8_t>{'m', 's', 'g'});
}

TEST(Priority, Construction) {
  const Priority p;
  EXPECT_EQ(p, Priority::DEFAULT);
  Priority p1(5);
  EXPECT_EQ(p1, 5);

  Priority p2(0);
  EXPECT_EQ(p2, 0);

  Priority p3(std::numeric_limits<unsigned int>::max());
  EXPECT_EQ(p3, std::numeric_limits<unsigned int>::max());
}

TEST(Priority, ImplicitConversion) {
  Priority p(7);
  unsigned int val = p;
  EXPECT_EQ(val, 7);

  // Test in boolean context
  if (p) {
    SUCCEED();
  } else {
    FAIL() << "Implicit conversion failed in boolean context";
  }
}

TEST(Priority, ComparisonOperators) {
  Priority low(1), medium(3), high(5);

  EXPECT_TRUE(low < medium);
  EXPECT_TRUE(medium <= medium);
  EXPECT_TRUE(high > medium);
  EXPECT_TRUE(medium >= medium);
  EXPECT_TRUE(medium == medium);
  EXPECT_TRUE(low != medium);
}

TEST(Priority, ConstexprContext) {
  constexpr Priority p1;
  constexpr Priority p2(4);
  static_assert(p1 == Priority::DEFAULT);
  static_assert(p2 == 4);
  static_assert(4 == p2);
  static_assert(Priority(2) < Priority(3));

  static_assert(Byte<char>);
  static_assert(Byte<signed char>);
  static_assert(Byte<unsigned char>);
  static_assert(Byte<int8_t>);
  static_assert(Byte<uint8_t>);
  static_assert(Byte<char8_t>);
  static_assert(Byte<bool>);

  struct S {};
  struct B {
    uint8_t b;
  };
  static_assert(!Byte<S>);
  static_assert(!Byte<B>);
  SUCCEED();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
