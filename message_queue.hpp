#pragma once

#include <fcntl.h>
#include <mqueue.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <expected>
#include <format>
#include <print>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace message_queue {

inline constexpr size_t DEFAULT_PERM = 0640;

// can be a template param
enum struct MqType {
  RECEIVER = O_RDONLY,
  SENDER = O_WRONLY,
  BIDIRECTIONAL = O_RDWR,
};

enum struct MqMode { BLOCKING = 0, NON_BLOCKING = O_NONBLOCK };

namespace detail {
using MqError = std::string;

template <typename T>
concept Byte = sizeof(T) == 1 && std::is_integral_v<T>;

template <typename C>
concept ByteContainer = requires(C c) {
  requires Byte<std::decay_t<decltype(c[0])>>;
  c.data();
  c.size();
  { c.begin() } -> std::contiguous_iterator;
  { c.end() } -> std::contiguous_iterator;
};
};  // namespace detail

class MessageQueue {
 public:
  struct Priority {
    constexpr static inline unsigned int DEFAULT = 3;
    constexpr Priority() noexcept = default;
    constexpr Priority(unsigned int priority) noexcept : priority_{priority} {}
    constexpr operator unsigned int() const { return priority_; }

   private:
    friend class MessageQueue;
    unsigned int priority_{DEFAULT};
  };

  struct Message {
    std::string contents;
    Priority priority;
  };

  explicit MessageQueue(std::string_view name,
                        MqMode mode = MqMode::NON_BLOCKING,
                        MqType type = MqType::BIDIRECTIONAL)
      : name_{name_sanity_check(name)},
        type_{type},
        mqdes_{mq_open(
            name_.data(),
            O_CREAT | std::to_underlying(type_) | std::to_underlying(mode),
            DEFAULT_PERM, nullptr)} {
    if (mqdes_ == -1) throw std::invalid_argument{parse_err(Operation::Open)};

    assert(get_attr());
    assert(mqdes_ && !errno);
  }

  explicit MessageQueue(std::string_view name, MqType type)
      : MessageQueue(name, MqMode::NON_BLOCKING, type) {}

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
    if (mqdes_ != -1) assert(mq_close(mqdes_) == 0 && !errno);
  }

  auto size() const -> size_t {
    return get_attr()
        .and_then(
            [this](std::monostate) -> std::expected<size_t, detail::MqError> {
              return static_cast<size_t>(attr_.mq_curmsgs);
            })
        .value();  // ok to crash?
  }

  // fixed after `mq_open()`
  auto max_size() -> size_t { return static_cast<size_t>(attr_.mq_maxmsg); }

  // fixed after `mq_open()`
  auto max_msgsize() -> size_t { return static_cast<size_t>(attr_.mq_msgsize); }

  auto capacity() -> size_t { return max_size() - size(); }

  auto is_empty() const -> size_t { return !size(); }

  auto name() const -> std::string_view { return name_; }

  auto type() const -> MqType { return type_; }

  auto mode() const -> MqMode {
    return get_attr()
        .and_then(
            [this](std::monostate) -> std::expected<MqMode, detail::MqError> {
              return (attr_.mq_flags | O_NONBLOCK) ? MqMode::NON_BLOCKING
                                                   : MqMode::BLOCKING;
            })
        .value();
  }

  template <detail::ByteContainer C>
  auto send(const C& msg, Priority priority = Priority::DEFAULT)
      -> std::expected<std::monostate, detail::MqError> {
    if (mq_send(mqdes_, std::bit_cast<const char*>(msg.data()), msg.size(),
                priority))
      return std::unexpected{parse_err(Operation::Send)};
    return {};
  }

  // for null-terminated arrays
  auto send(const detail::Byte auto* msg,
            Priority priority = Priority::DEFAULT) {
    return send(std::span(msg, strlen(msg)), priority);
  }

  auto receive() -> std::expected<Message, detail::MqError> {
    Priority priority;

    // reserve() here is UB: data() ptr valid range: (data(), data() + size];
    // reserve() doesn't change the size
    auto msg = std::string(max_msgsize(), '\0');
    if (auto ret =
            mq_receive(mqdes_, msg.data(), msg.size(), &priority.priority_);
        ret != -1) {
      auto size = static_cast<size_t>(ret);
      msg.resize(size);
      msg.shrink_to_fit();
      assert(msg.size() == size);
      return Message{std::move(msg), priority};
    }
    return std::unexpected{parse_err(Operation::Receive)};
  }

  auto clear() -> std::expected<std::monostate, detail::MqError> {
    // size() must not be re-called during comparison
    for (auto i = 0uz, sz = size(); i < sz; ++i)
      if (auto e = receive(); !e) std::unexpected{e.error()};
    return {};
  }

 private:
  enum struct Operation { Open, Close, Send, Receive, GetAttr };
  mutable mq_attr attr_{};
  std::string_view name_{};
  MqType type_{};
  mqd_t mqdes_{-1};

  auto get_attr() const -> std::expected<std::monostate, detail::MqError> {
    if (mq_getattr(mqdes_, &attr_))
      return std::unexpected{parse_err(Operation::GetAttr)};
    return {};
  }

  static auto name_sanity_check(std::string_view name) -> std::string_view {
    if (!name.starts_with('/') ||
        name.find_first_of('/') != name.find_last_of('/') || name.size() == 1 ||
        name.size() + 1 >= static_cast<size_t>(pathconf("/", _PC_NAME_MAX)))
      throw std::invalid_argument("invalid mq name");

    return name;
  }

  auto parse_err(Operation op) const -> detail::MqError {
    using enum Operation;
    static const auto err_map =
        std::unordered_map<Operation,
                           std::unordered_map<int, std::string_view>>{
            {Open,
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

    auto err = std::exchange(errno, 0);
    return std::format("Error: operation {} with errno {}: {}",
                       std::to_underlying(op), err, err_map.at(op).at(err));
  }
};
};  // namespace message_queue

template <>
struct std::formatter<message_queue::MessageQueue::Priority>
    : std::formatter<std::decay_t<
          decltype(message_queue::MessageQueue::Priority::DEFAULT)>> {};

using namespace message_queue;  // if main.cpp is not to be modified
