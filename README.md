# MQ-Wrapper

A C++23 header-only, type-safe and ergonomic abstraction over the Linux message
queue APIs.

# Features

- blocking/non-blocking, uni- or bidirectional
- optionally time-based send/receive
- compile-time byte buffer type checking via `concept`
- functional error handling via `std::expected`
- strongly typed message priority with compile-time bound-checked values
- human-readable error diagnostics mapped from `errno`
- move-only semantic

# Usage Example

```cpp
#include "message_queue.hpp"
using namespace message_queue;

auto mq = MessageQueue{"/my_queue"};
// auto mq = MessageQueue{"/myqueue", MqMode::NON_BLOCKING, MqType::BIDIRECTIONAL};

auto result = mq.send("hello", MessageQueue::Priority{10});

const auto& [contents, priority] = mq.receive().value();
```

# Key API

- `mq.send(data, priority [, timeout])` — send bytes/buffer with optional timeout
- `mq.receive([timeout])` — retrieve message and its priority
- `MessageQueue::unlink(name)` — remove message queue from system
- `MessageQueue::Builder` — construction/reset APIs mapping constructor
  exception to `std::expected`

Tested on Arch Linux (btw.)
