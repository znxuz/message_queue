#pragma once

#include <fcntl.h>
#include <mqueue.h>

#include <cassert>
#include <cerrno>
#include <format>
#include <span>
#include <stdexcept>
#include <utility>

namespace message_queue {
enum struct MQ_MODE { BLOCKING = 0, NON_BLOCKING = O_NONBLOCK };

enum struct MQ_TYPE {
  RECEIVER = O_RDONLY,
  SENDER = O_WRONLY,
  BIDIRECTIONAL = O_RDWR,
};

using namespace std::literals;
using enum MQ_MODE;
using enum MQ_TYPE;

inline constexpr size_t DEFAULT_PERM = 0640;

class MessageQueue {
 public:
  class Priority {
   public:
    constexpr Priority(unsigned int priority) : priority_{priority} {}
    constexpr operator unsigned int() { return priority_; }

   private:
    unsigned int priority_;
  };

  explicit MessageQueue(std::string_view name, MQ_TYPE type = BIDIRECTIONAL,
                        MQ_MODE mode = NON_BLOCKING)
      : name_{name},
        type_{type},
        mode_{mode},
        mqdes_{mq_open(
            name_.data(),
            O_CREAT | std::to_underlying(type_) | std::to_underlying(mode_),
            DEFAULT_PERM, nullptr)} {
    if (mqdes_ == -1) error_handler();
    assert(mqdes_ && errno == 0);
  }

  explicit MessageQueue(std::string_view name, MQ_MODE mode)
      : MessageQueue(name, BIDIRECTIONAL, mode) {}

  ~MessageQueue() { assert(mq_close(mqdes_) == 0 && errno == 0); }

  size_t size() const { return static_cast<size_t>(attr_.mq_curmsgs); }

  size_t is_empty() const { return !size(); }

  // TODO templatazie elem
  template <typename T>
  void send(std::span<const T> data) {
    if (mq_send(mqdes_, reinterpret_cast<const char*>(data.data()), data.size(),
                0))
      error_handler();
  }

  // TODO overload const char/uint8_t*

 private:
  std::string_view name_;
  MQ_TYPE type_;
  MQ_MODE mode_;
  mqd_t mqdes_;
  mq_attr attr_;

  void update() { mq_getattr(mqdes_, &attr_); }

  void error_handler() {
    // TODO: human readable error output based on errno

    throw std::invalid_argument(
        std::format("errno {} on mq_open: TODO", errno));
  }
};
};  // namespace message_queue

template <>
struct std::formatter<message_queue::MessageQueue::Priority>
    : std::formatter<unsigned int> {};

using namespace message_queue;  // if main.cpp is not to be modified
