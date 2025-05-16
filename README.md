[![wakatime](https://wakatime.com/badge/user/77eda5cb-f41d-45da-a208-715b0faa4269/project/2fdc66e6-17db-4272-a608-d1ae0e7bdf1e.svg)](https://wakatime.com/badge/user/77eda5cb-f41d-45da-a208-715b0faa4269/project/2fdc66e6-17db-4272-a608-d1ae0e7bdf1e)

# manpage notes

- POSIX-confirm name `std::string_view` sanity check:
	- always start with forward slash & none forward slash afterwards
	- null-terminated string to `getconf NAME_MAX /` (filesystem-specific)
	- TODO maybe auto-append the initial slash if not exist
- `struct mq_attr`:
	- `mq_flags`: (ignored for `mq_open()`) flags set for the queue:
		- only `O_NONBLOCK`: non-blocking for `mq_receive()` or `mq_send()`
	- `mq_maxmsg`: max. # of msgs stored in the queue; **must be non-zero**
	- `mq_msgsize`: size (bytes) of each msg; **must be non-zero**
	-  `mq_curmsgs`: # of msgs currently in the queue
- `mq_open()`:
	- always bidirectional based on the API
	- attach if exists, otherwise create -> no `O_CLOEXEC`
	- `mq_attr`:
		- `mq_flags`: ignored
		- `mq_maxmsg`: derive from template analog to `std::array`
		- `mq_msgsize`: derive from template: [ ] `sizeof` safe?
		- `mq_curmsgs`: ignored
	- [ ] respect `errno` and make them human-readable: -> `man mq_open:ERRORS`
	- return `(mqd_t)-1` on error -> TODO exceptions?
- `mq_send()`:
	- [ ] overloads with `const T&`, `T&&`, maybe analog `emplace_back()` too?
	- [ ] overloads with optional timeout -> `std::chrono`
	- [ ] `std::expected` on empty queue in non-blocking mode with `EAGAIN`
	- `mq_send()` takes `const char*`: `std::bit_cast`
	- priority:
		- range from `0(low)` to `$(getconf MQ_PRIO_MAX)` or
		  `sysconf(_SC_MQ_PRIO_MAX)-1(high)`: 32768 on Linux, 31 with POSIX.1
		- [ ] as nested/inner class with bound checking
		- [ ] overload `<<`, `>>` and also for `std::println()`
	- [ ] respect `errno` -> `man mq_send:ERRORS`
- `mq_receive()`:
	- [ ] should return (msg, #) -> `std::expected<std::pair<...>, E>`
	- RVO via `std::bit_cast` on return?
	- optional timeout: TODO maybe as optional template param for non-blocking
	  behavior and ` if constexpr ...` for both send and receive
	- [ ] respect `errno` -> `man mq_receive:ERRORS`
- `mq_notify()`: notify upon msg arrival on a **previously empty** queue:
	- `SIGEV_NONE`: only register, no notification
	- `SIGEV_SIGNAL`: sending the specified signal
	- `SIGEV_THREAD`: invoke `sigev_notify_function` TODO template queue size
	- only one process can register from a msg queue **once** per `mq_notify()`
	- unregister with `(struct sigevent*)NULL`
	- no notification if `mq_receive` is waiting on new msg
	- errno and example: `man mq_notify`
- `mq_close()`:
	- just close after use; no delete
	- or the notification is removed if exists
- `mq_unlink()`:
	- close and delete -> call unlink in dtor when supplied with `O_CREAT`?
- `mq_getattr()`: get the attributes of a msg queue
- `mq_setattr()`: only `mq_flags` i.e. `O_NONBLOCK` can be set

# tests

- [ ] test bidirectional
- [ ] test non-padded/padded/polymorphic objects with inherited paddings
- [ ] test with producer/consumer threads

# unix tools

- `ipcs -q`
- `cat /dev/mqueue/<queue_name>`: size in bytes (not in messages)
- `/proc/sys/fs/mqueue/`: config dir
