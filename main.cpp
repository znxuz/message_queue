#include <cassert>
#include <iostream>
#include "message_queue.hpp"

static constexpr auto queue_name = "/my_queue";

void use_queue(MessageQueue queue) {
    queue.send("Test", MessageQueue::Priority{ 2 });
    assert(queue.receive().value().contents == "Test");
}

int main() {
    auto queue1 = MessageQueue{ queue_name };
    assert(queue1.size() == 0);
    queue1.send("Hello, world!");
    assert(!queue1.is_empty());
    assert(queue1.size() == 1);

    auto queue2 = MessageQueue{ queue_name };
    assert(!queue2.is_empty());
    assert(queue2.size() == 1);

    const auto received_message = queue2.receive();
    assert(received_message.has_value());
    assert(queue2.is_empty());
    assert(queue2.size() == 0);
    const auto& [contents, priority] = received_message.value();
    assert(contents == "Hello, world!");
    assert(priority == MessageQueue::Priority{ 3 });

    // Additional task: Call the function `use_queue()` and pass the `queue2` object.
    use_queue(std::move(queue2)); // <- remove me!

    // Additional task: Make the following line compile.
    std::cout << priority << '\n';
    std::println("{}", priority);
}
