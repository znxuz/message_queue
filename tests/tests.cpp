#include <gtest/gtest.h>
#include <mqueue.h>

#include <bit>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include "../message_queue.hpp"

using namespace message_queue;
using namespace std::literals;

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

TEST_F(MessageQueueTest, Move) {
  auto name = std::string(mq.name());
  auto mode = mq.mode();
  auto type = mq.type();
  auto max_size = mq.max_size();
  auto max_msgsize = mq.max_msgsize();

  auto new_mq = std::move(mq);
  EXPECT_EQ(new_mq.name(), name);
  EXPECT_EQ(new_mq.mode(), mode);
  EXPECT_EQ(new_mq.type(), type);
  EXPECT_EQ(new_mq.max_size(), max_size);
  EXPECT_EQ(new_mq.max_msgsize(), max_msgsize);

  EXPECT_TRUE(new_mq.send("moin"));
  EXPECT_EQ(new_mq.size(), 1);
  EXPECT_EQ(new_mq.receive().value().contents, "moin"s);
}

TEST_F(MessageQueueTest, Attr) {
  auto from_file = [](const std::string& filename) static {
    std::ifstream file(filename);
    size_t value;
    file >> value;
    return value;
  };

  auto mq_kernel_path = "/proc/sys/fs/mqueue"s;
  EXPECT_EQ(mq.max_size(), from_file(mq_kernel_path + "/msg_default"s));
  EXPECT_EQ(mq.max_msgsize(), from_file(mq_kernel_path + "/msgsize_default"s));
  EXPECT_EQ(mq.mode(), MqMode::NON_BLOCKING);
}

TEST_F(MessageQueueTest, EmptyError) {
  auto ret = mq.receive();
  EXPECT_FALSE(ret);

  EXPECT_EQ(ret.error(), "Error: operation 3 with errno 11: queue is empty"s);
}

TEST_F(MessageQueueTest, FullError) {
  for (auto i : std::views::iota(0uz, mq.max_size()))
    EXPECT_TRUE(mq.send(""sv));

  auto ret = mq.send("");
  EXPECT_FALSE(ret);
  EXPECT_EQ(ret.error(), "Error: operation 2 with errno 11: queue is full"s);

  EXPECT_TRUE(mq.clear());
}

TEST_F(MessageQueueTest, SenderError) {
  auto sender = MessageQueue{"/sender", MqType::SENDER};
  auto ret = sender.receive();
  EXPECT_FALSE(ret);
  EXPECT_EQ(ret.error(),
            "Error: operation 3 with errno 9: invalid mq fd, or the queue is "
            "not opened for receiving");
  mq_unlink(sender.name().data());
}

TEST_F(MessageQueueTest, ReceiverError) {
  auto receiver = MessageQueue{"/receiver", MqType::RECEIVER};
  auto ret = receiver.send("");
  EXPECT_FALSE(ret);
  EXPECT_EQ(ret.error(),
            "Error: operation 2 with errno 9: invalid mq fd, or the queue is "
            "not opened for sending");
  mq_unlink(receiver.name().data());
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
  auto round_trip_check = [](MessageQueue& mq, const auto& msg) static {
    EXPECT_TRUE(mq.send(msg));
    auto ret = mq.receive();
    EXPECT_TRUE(ret);
    const auto& [str, prio] = ret.value();
    EXPECT_EQ(prio, Priority::DEFAULT);
    for (auto i = 0uz; i < str.size(); ++i) EXPECT_EQ(msg[i], str[i]);
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

TEST_F(MessageQueueTest, SendPolymorphic) {
  struct B {
    size_t a;
    short b;
    bool operator<=>(const B&) const = default;
  };
  static_assert(sizeof(B) == 16);
  struct D : B {
    size_t c;
    short d;
    bool operator<=>(const D&) const = default;
  };
  static_assert(sizeof(D) == 32);

  auto d = D{{0xa13f, 0x2}, 0xafe1fd, 0x5};
  EXPECT_TRUE(mq.send(std::span(std::bit_cast<const char*>(&d), sizeof(d))));
  auto ret = mq.receive();
  EXPECT_TRUE(ret);
  auto received = *std::bit_cast<const D*>(ret.value().contents.data());

  EXPECT_EQ(d, received);
}

TEST_F(MessageQueueTest, SendPolymorphicPacked) {
  struct B {
    size_t a;
    short b;
    bool operator<=>(const B&) const = default;
  } __attribute__((packed));
  static_assert(sizeof(B) == 10);
  struct D : B {
    size_t c;
    short d;
    bool operator<=>(const D&) const = default;
  } __attribute__((packed));
  static_assert(sizeof(D) == 20);

  auto d = D{{0xa13f, 0x2}, 0xafe1fd, 0x5};
  EXPECT_TRUE(mq.send(std::span(std::bit_cast<const char*>(&d), sizeof(d))));
  auto ret = mq.receive();
  EXPECT_TRUE(ret);
  auto received = *std::bit_cast<const D*>(ret.value().contents.data());

  EXPECT_EQ(d, received);
}

TEST_F(MessageQueueTest, ProducerConsumer) {
  static auto payload = "moin"sv;

  auto producer = [](std::string_view name) static {
    auto mq = MessageQueue(name, MqType::SENDER);
    EXPECT_TRUE(mq.send(payload));
  };
  auto consumer = [](std::string_view name) static {
    using namespace message_queue;
    auto mq = MessageQueue(name, MqType::RECEIVER);
    while (true) {
      if (auto ret = mq.receive(); ret) {
        EXPECT_EQ(payload, ret.value().contents);
        return;
      }
    }
  };

  auto p = std::jthread{producer, mq.name()};
  auto c = std::jthread{consumer, mq.name()};
}

TEST(Concept, ByteSequenceTest) {
  // copied from MessageQueue
  static constexpr auto get_ptr_and_size =
      [](const detail::ByteSequence auto& sequence) static {
        using sequence_type = std::decay_t<decltype(sequence)>;

        if constexpr (requires { requires detail::StlSequence<sequence_type>; })
          return std::pair{sequence.data(), sequence.size()};
        else
          return std::pair{sequence, std::strlen(sequence)};
      };

  auto check = [](const detail::ByteSequence auto& sequence) static {
    using sequence_type = std::decay_t<decltype(sequence)>;
    const auto [data, size] = get_ptr_and_size(sequence);

    if constexpr (requires { requires detail::StlSequence<sequence_type>; }) {
      EXPECT_EQ(data, sequence.data());
      EXPECT_EQ(size, sequence.size());
    } else {
      EXPECT_EQ(data, sequence);
      EXPECT_EQ(size, std::strlen(sequence));
    }
  };

  check(std::vector<int8_t>{1, 2, 3});
  check(std::vector<uint8_t>{1, 2, 3});
  check(std::vector<char>{1, 2, 3});
  check(std::vector<unsigned char>{1, 2, 3});
  check(std::array<int8_t, 3>{1, 2, 3});
  check(std::array<uint8_t, 3>{1, 2, 3});
  check(std::array<char, 3>{'a', 'b', 'c'});
  check(std::array<unsigned char, 3>{'a', 'b', 'c'});
  check(std::span{"hello"sv});
  // check(std::deque<char>{}); // not as continuous mem
  // check(std::set<char>{});
  check("hello"s);
  check("hello"sv);

  check("hello");  // null-terminated char arr[] by default
  const char char_arr[] = {'a', 'b', 'c', '\0'};
  check(char_arr);
  const char* char_ptr = "hello";
  check(char_ptr);
  check(std::span{char_ptr, 2}); // if ptr/arr isn't null-terminated

  const uint8_t arr[] = {1, 2, '\0', 4};
  const uint8_t* ptr = arr;
  // compile error, as it should be
  // check(arr);
  // check(ptr);
  auto sp = std::span{arr};
  EXPECT_EQ(sp.size(), sizeof(arr));
  check(sp);
}

TEST(MessageQueueBuilder, TestBuilder) {
  auto mode = MqMode::BLOCKING;
  auto type = MqType::RECEIVER;
  auto name = "/blocking_mq"sv;

  mq_unlink(name.data());  // just so that the test always starts clean

  auto ret = MessageQueue::Builder()
                 .set_mode(mode)
                 .set_type(type)
                 .set_name(name)
                 .reset(true)
                 .build();
  EXPECT_FALSE(ret);  // should fail cuz queue doesn't exist yet to be unlinked

  ret = MessageQueue::Builder()
            .set_mode(mode)
            .set_type(type)
            .set_name(name)
            .reset(false)
            .build();
  EXPECT_TRUE(ret);

  const auto& mq = *ret;
  EXPECT_EQ(mode, mq.mode());
  EXPECT_EQ(type, mq.type());
  EXPECT_EQ(name, mq.name());

  ret = MessageQueue::Builder()
            .set_mode(mode)
            .set_type(type)
            .set_name(name)
            .reset(true)
            .build();
  EXPECT_TRUE(ret);
  EXPECT_TRUE(MessageQueue::unlink(name));
}

TEST(Priority, Construction) {
  const Priority p;
  EXPECT_EQ(p, Priority::DEFAULT);
  EXPECT_EQ(Priority{5}, 5);
  EXPECT_EQ(Priority{0}, 0);

  EXPECT_THROW(Priority{32768}, std::invalid_argument);
  EXPECT_THROW(Priority{static_cast<unsigned int>(-1)}, std::invalid_argument);
  EXPECT_EQ(Priority{32767}, 32767);
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

TEST(Constexpr, Constexpr) {
  constexpr auto p1 = Priority{};
  constexpr auto p2 = Priority{4};
  static_assert(p1 == Priority::DEFAULT);
  static_assert(p2 == 4);
  static_assert(4 == p2);
  static_assert(Priority{2} < Priority{3});
  constexpr auto p3 = Priority{32767};
  static_assert(p3 == 32767);
  // constexpr auto compile_err = Priority{32768};

  using namespace message_queue::detail;
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
