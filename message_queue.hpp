#pragma once

#include <fcntl.h>
#include <mqueue.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <expected>
#include <print>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace message_queue {

enum struct MqType {
  RECEIVER = O_RDONLY,
  SENDER = O_WRONLY,
  BIDIRECTIONAL = O_RDWR,
};

enum struct MqMode { BLOCKING = 0, NON_BLOCKING = O_NONBLOCK };

using MqError = int;

// TODO: move to _detail ns
template <typename T>
concept Byte = sizeof(T) == 1 && std::is_integral_v<T>;

inline constexpr size_t DEFAULT_PERM = 0640;

// TODO: rm after develop
using enum MqMode;
using enum MqType;

class MessageQueue {
 public:
  struct Priority {
    constexpr static inline unsigned int DEFAULT = 3;
    constexpr Priority() = default;
    constexpr Priority(unsigned int priority) : priority_{priority} {}
    constexpr operator unsigned int() { return priority_; }
    constexpr operator unsigned int() const { return priority_; }
    constexpr auto operator<=>(const Priority&) const = default;

   private:
    friend class MessageQueue;
    unsigned int priority_{DEFAULT};
  };

  struct Message {
    std::string contents;
    Priority priority;

    friend class MessageQueue;
  };

  explicit MessageQueue(std::string_view name, MqMode mode = NON_BLOCKING,
                        MqType type = BIDIRECTIONAL)
      : name_{name_sanity_check(name)},
        type_{type},
        mqdes_{mq_open(
            name_.data(),
            O_CREAT | std::to_underlying(type_) | std::to_underlying(mode),
            DEFAULT_PERM, nullptr)} {
    if (mqdes_ == -1) {
      log_error(Operation::Open);
      throw std::invalid_argument("failed to create a message queue");
    }
    assert(mqdes_ && errno == 0);
  }

  explicit MessageQueue(std::string_view name, MqType type)
      : MessageQueue(name, NON_BLOCKING, type) {}

  MessageQueue(MessageQueue&& rhs) noexcept {
    using std::swap;
    *this = std::move(rhs);
  }

  MessageQueue& operator=(MessageQueue&& rhs) noexcept {
    using std::swap;
    swap(attr_, rhs.attr_);
    swap(name_, rhs.name_);
    swap(type_, rhs.type_);
    swap(mqdes_, rhs.mqdes_);

    return *this;
  }

  ~MessageQueue() {
    if (mqdes_ != -1) assert(mq_close(mqdes_) == 0 && errno == 0);
  }

  auto size() const -> size_t {
    return update()
        .and_then([this](std::monostate) -> std::expected<size_t, MqError> {
          return static_cast<size_t>(attr_.mq_curmsgs);
        })
        .value();  // ok to crash?
  }

  auto is_empty() const -> size_t { return !size(); }

  template <Byte B>
  auto send(std::span<const B> msg, Priority priority = Priority::DEFAULT)
      -> std::expected<std::monostate, MqError> {
    if (mq_send(mqdes_, std::bit_cast<const char*>(msg.data()), msg.size(),
                priority))
      return std::unexpected{log_error(Operation::Send)};
    return {};
  }

  // for null-terminated arrays
  auto send(const Byte auto* msg, Priority priority = Priority::DEFAULT) {
    return send(std::span(msg, strlen(msg)), priority);
  }

  auto receive() -> std::expected<Message, MqError> {
    Priority priority;

    auto max_msg_size =
        update()
            .and_then([this](std::monostate) -> std::expected<size_t, MqError> {
              return static_cast<size_t>(attr_.mq_msgsize);
            })
            .value();
    auto msg = std::string{};
    msg.resize(max_msg_size);  // reserve() here is UB:
                               // data ptr valid range: (data(), data() + size]
    if (auto ret =
            mq_receive(mqdes_, msg.data(), max_msg_size, &priority.priority_);
        ret != -1) {
      auto size = static_cast<size_t>(ret);
      msg.resize(size);
      msg.shrink_to_fit();
      assert(msg.size() == size);
      return Message{std::move(msg), priority};
    }
    return std::unexpected{log_error(Operation::Receive)};
  }

  auto type() const -> MqType { return type_; }

  auto is_blocking() const -> bool {
    return update()
        .and_then([this](std::monostate) -> std::expected<bool, MqError> {
          return !(attr_.mq_flags | O_NONBLOCK);
        })
        .value();
  }

 private:
  enum struct Operation { Open, Close, Send, Receive, GetAttr };
  mutable mq_attr attr_{};
  std::string_view name_{};
  MqType type_{};
  mqd_t mqdes_{-1};

  auto update() const -> std::expected<std::monostate, MqError> {
    if (mq_getattr(mqdes_, &attr_))
      return std::unexpected{log_error(Operation::GetAttr)};
    return {};
  }

  static auto name_sanity_check(std::string_view name) -> std::string_view {
    if (!name.starts_with('/') ||
        name.find_first_of('/') != name.find_last_of('/') || name.size() < 1 ||
        name.size() >= static_cast<size_t>(pathconf("/", _PC_NAME_MAX)))
      throw std::invalid_argument("invalid mq name");

    return name;
  }

  auto log_error(Operation op) const -> MqError {
    using enum Operation;
    // TODO: std::vector/array based instead of umap
    static const std::unordered_map<Operation,
                                    std::unordered_map<int, std::string_view>>
        err_map{{Open,
                 {{EACCES, "insufficient permission"},
                  {EEXIST, "queue with the same name already exist"},
                  {EMFILE, "per-process limit on the number of fds is reached"},
                  {ENFILE, "system-wide limit on the number of fds is reached"},
                  {ENOENT, "queue doesn't exist"},
                  {ENOMEM, "insufficient memory"},
                  {ENOSPC, "insufficient space"}}},
                {Close, {{EBADF, "invalid mq fd"}}},
                {Send,
                 {{EAGAIN, "queue is full"},
                  {EBADF, "invalid mq fd"},
                  {EINTR, "interrupted by a single handler"},
                  {EINVAL, "TODO: not implemented"},
                  {EMSGSIZE, "contained message length greater than max. size"},
                  {ETIMEDOUT, "TODO: not implemented"}}},
                {Receive,
                 {{EAGAIN, "queue is empty"},
                  {EBADF, "invalid mq fd"},
                  {EINTR, "interrupted by a single handler"},
                  {EINVAL, "TODO: time-based api not implemented"},
                  {EMSGSIZE, "given message length less than max. size"},
                  {ETIMEDOUT, "TODO: time-based api not implemented"}}},
                {GetAttr,
                 {{EBADF, "invalid mq fd"},
                  {EINVAL, "mq_flags contains more than O_NONBLOCK"}}}};

    std::println(stderr, "operation {} with errno {}: {}",
                 std::to_underlying(op), errno, err_map.at(op).at(errno));
    return errno;
  }
};
};  // namespace message_queue

template <>
struct std::formatter<message_queue::MessageQueue::Priority>
    : std::formatter<unsigned int> {};

using namespace message_queue;  // if main.cpp is not to be modified
