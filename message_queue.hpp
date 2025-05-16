#pragma once

#include <fcntl.h>
#include <mqueue.h>

#include <cassert>
#include <cerrno>
#include <cstring>
#include <expected>
#include <format>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace message_queue {

enum struct MqError {
  FULL,
  EMPTY,
};

enum struct MqType {
  RECEIVER = O_RDONLY,
  SENDER = O_WRONLY,
  BIDIRECTIONAL = O_RDWR,
};

enum struct MqMode { BLOCKING = 0, NON_BLOCKING = O_NONBLOCK };

using namespace std::literals;

using enum MqError;
using enum MqMode;
using enum MqType;

inline constexpr size_t DEFAULT_PERM = 0640;

// TODO: move to _detail ns
template <typename T>
concept Byte = sizeof(T) == 1 && std::is_integral_v<T>;

class MessageQueue {
 public:
  struct Priority {
    constexpr static inline unsigned int DEFAULT = 3;
    constexpr Priority() = default;
    constexpr Priority(unsigned int priority) : priority_{priority} {}
    constexpr operator unsigned int() { return priority_; }
    constexpr auto operator<=>(const Priority&) const = default;

   private:
    friend class MessageQueue;
    unsigned int priority_{DEFAULT};
  };

  explicit MessageQueue(std::string_view name, MqMode mode = NON_BLOCKING,
                        MqType type = BIDIRECTIONAL)
      : name_{name},
        type_{type},
        mqdes_{mq_open(
            name_.data(),
            O_CREAT | std::to_underlying(type_) | std::to_underlying(mode),
            DEFAULT_PERM, nullptr)} {
    if (mqdes_ == -1) error_handler();
    assert(mqdes_ && errno == 0);
  }

  explicit MessageQueue(std::string_view name, MqType type)
      : MessageQueue(name, NON_BLOCKING, type) {}

  ~MessageQueue() { assert(mq_close(mqdes_) == 0 && errno == 0); }

  auto size() const -> size_t {
    update();
    return static_cast<size_t>(attr_.mq_curmsgs);
  }

  auto is_empty() const -> size_t { return !size(); }

  template <Byte B>
  auto send(std::span<const B> msg) -> std::expected<std::monostate, MqError> {
    if (mq_send(mqdes_, std::bit_cast<const char*>(msg.data()), msg.size(),
                Priority::DEFAULT))
      return std::unexpected{error_handler()};
    return std::monostate{};
  }

  // for null-terminated arrays
  auto send(const Byte auto* msg) { send(std::span(msg, strlen(msg))); }

  auto receive() -> std::expected<std::pair<std::string, Priority>, MqError> {
    Priority priority;

    auto max_msg_size = static_cast<size_t>(attr_.mq_msgsize);
    auto msg = std::string{};
    msg.resize(
        max_msg_size);  // reserve() here is UB:
                        // data() ptr valid range: (data(), data() + size]
    if (auto ret =
            mq_receive(mqdes_, msg.data(), max_msg_size, &priority.priority_);
        ret != -1) {
      auto size = static_cast<size_t>(ret);
      msg.resize(size);
      msg.shrink_to_fit();
      assert(msg.size() == size);
    } else {
      return std::unexpected{error_handler()};
    }

    return std::make_pair(std::move(msg), priority);
  }

  auto type() const -> MqType { return type_; }

 private:
  mutable mq_attr attr_{};
  std::string_view name_;
  MqType type_;
  mqd_t mqdes_;

  auto update() const -> void { mq_getattr(mqdes_, &attr_); }

  auto error_handler() -> MqError {
    // TODO: human readable error output based on caller & errno
    throw std::invalid_argument(std::format("errno {}: TODO", errno));
  }
};
};  // namespace message_queue

template <>
struct std::formatter<message_queue::MessageQueue::Priority>
    : std::formatter<unsigned int> {};

using namespace message_queue;  // if main.cpp is not to be modified
