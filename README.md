# todos

- add reset flag for resetting the queue upon reopen
- factory method returning `std::expected`
- time-based api

# manpage notes

- POSIX-confirm name `std::string_view` sanity check:
	- always start with forward slash & none forward slash afterwards
	- null-terminated string to `getconf NAME_MAX /` (filesystem-specific)
- `struct mq_attr`:
	- `mq_flags`: (ignored for `mq_open()`) flags set for the queue:
		- only `O_NONBLOCK`: non-blocking for `mq_receive()` or `mq_send()`
	- `mq_maxmsg`: max. # of msgs stored in the queue; **must be non-zero**
	- `mq_msgsize`: max. size (bytes) of each msg; **must be non-zero**
	-  `mq_curmsgs`: # of msgs currently in the queue
- `mq_open()`:
	- always bidirectional based on the API; attach if exists
	- `mq_attr`:
		- `mq_flags`: ignored
		- `mq_maxmsg`: defaulted to linux kernel config /proc
		- `mq_msgsize`: defaulted to linux kernel config /proc
		- `mq_curmsgs`: ignored
	- [X] respect `errno` and make them human-readable: -> `man mq_open:ERRORS`
	- [X] `(mqd_t)-1` on error -> throw in ctor
- `mq_send()`:
	- [ ] overloads with `const T&`, `T&&`, maybe analog `emplace_back()` too?
	- [ ] overloads with optional timeout -> `std::chrono`
	- `mq_send()` takes `const char*`: `std::bit_cast`
	- priority:
		- range from `0(low)` to `$(getconf MQ_PRIO_MAX)` or
		  `sysconf(_SC_MQ_PRIO_MAX)-1(high)`: 32768 on Linux, 31 with POSIX.1
		- [ ] as nested/inner class with bound checking
		- [X] serialize to `cout`: impl. conversion
	- [X] respect `errno` -> `man mq_send:ERRORS`
- `mq_receive()`:
	- [X] should return (msg, priority) -> `std::expected<std::pair<...>, E>`
	- [X] RVO via `Message` inner struct
	- [ ] overloads with optional timeout -> `std::chrono`
	- [X] respect `errno` -> `man mq_receive:ERRORS`
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

- [X] test bidirectional
- [X] test non-padded/padded/polymorphic objects with inherited paddings
- [X] test with producer/consumer threads
- [X] test priority comparisons

# unix tools

- `ipcs -q`
- `cat /dev/mqueue/<queue_name>`: size in bytes (not in messages)
- `/proc/sys/fs/mqueue/`: config dir

[![wakatime](https://wakatime.com/badge/user/77eda5cb-f41d-45da-a208-715b0faa4269/project/2fdc66e6-17db-4272-a608-d1ae0e7bdf1e.svg)](https://wakatime.com/badge/user/77eda5cb-f41d-45da-a208-715b0faa4269/project/2fdc66e6-17db-4272-a608-d1ae0e7bdf1e)
