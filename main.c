#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mqueue.h>
#include <sys/stat.h> /* For file permission macros like `S_IRUSR`. */

/* The name of the queue must start with a forward slash and must not
   contain any other forward slashes after that. */
#define QUEUE_NAME "/my_queue"

int main(int argc, char* argv[]) {
    int q1, q2;
    struct mq_attr attrs;
    int failed = 0;
    ssize_t num_bytes;
    unsigned int priority;
    char* buffer;

    q1 = mq_open(QUEUE_NAME, O_RDWR | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR | S_IRGRP, NULL);
    if (q1 == -1) {
        fprintf(stderr, "Failed to create message queue: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    if (mq_getattr(q1, &attrs) == -1) {
        fprintf(stderr, "Failed to get queue attributes: %s\n", strerror(errno));
        failed = 1;
        goto cleanup_q1;
    }
    assert(attrs.mq_curmsgs == 0 && "Expected queue to be empty.\n");

    const char* const text = "Hello, world!";
    if (mq_send(q1, text, strlen(text), 3U) != 0) {
        fprintf(stderr, "Failed to send message: %s\n", strerror(errno));
        failed = 1;
        goto cleanup_q1;
    }
    if (mq_getattr(q1, &attrs) == -1) {
        fprintf(stderr, "Failed to get queue attributes: %s\n", strerror(errno));
        failed = 1;
        goto cleanup_q1;
    }
    assert(attrs.mq_curmsgs == 1 && "Expected queue to contain one message.");

    q2 = mq_open(QUEUE_NAME, O_RDWR | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR | S_IRGRP, NULL);
    if (q2 == -1) {
        fprintf(stderr, "Failed to create message queue: %s\n", strerror(errno));
        failed = 1;
        goto cleanup_q1;
    }

    /* The buffer must be big enough to be able to hold a message with maximum size. */
    buffer = malloc(attrs.mq_msgsize);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate buffer memory.\n");
        failed = 1;
        goto cleanup_q2;
    }

    do {
        num_bytes = mq_receive(q2, buffer, attrs.mq_msgsize, &priority);
    } while (num_bytes == -1 && errno == EAGAIN);

    if (num_bytes == -1) {
        fprintf(stderr, "Failed to receive message: %s\n", strerror(errno));
        failed = 1;
        goto cleanup_buffer;
    }

    assert(
        num_bytes == strlen(text)
        && strncmp(buffer, text, num_bytes) == 0
        && "Expected to receive 'Hello, world!' message."
    );

    if (mq_getattr(q2, &attrs) == -1) {
        fprintf(stderr, "Failed to get queue attributes: %s\n", strerror(errno));
        failed = 1;
        goto cleanup_buffer;
    }
    assert(attrs.mq_curmsgs == 0 && "Expected queue to be empty.\n");

cleanup_buffer:
    free(buffer);

cleanup_q2:
    if (mq_close(q2) == -1) {
        fprintf(stderr, "Failed to close queue: %s\n", strerror(errno));
        failed = 1;
    }

cleanup_q1:
    if (mq_close(q1) == -1) {
        fprintf(stderr, "Failed to close queue: %s\n", strerror(errno));
        failed = 1;
    }

    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
