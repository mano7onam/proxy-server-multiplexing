//
// Created by mano on 10.12.16.
//

#include "Buffer.h"

Buffer::Buffer(size_t size) {
    this->size = size;

    start = 0;
    end = 0;

    buf = (char*) malloc(size);

    if (NULL == buf) {
        perror("malloc");
        is_correct = false;
        exit(EXIT_FAILURE);
    }

    is_correct = true;
}

int Buffer::do_resize(size_t new_size) {
    char * new_buf = (char *)realloc(buf, new_size);

    if (NULL == new_buf) {
        perror("realloc");
        free(buf);
        exit(EXIT_FAILURE);
    }

    buf = new_buf;
    size = new_size;

    return RESULT_CORRECT;
}

int Buffer::do_move_end(ssize_t received) {
    fprintf(stderr, "Move end\n");

    end += received;

    if (end == size) {
        return do_resize(size * 2);
    }

    fprintf(stderr, "Done moving\n");
    return RESULT_CORRECT;
}

void Buffer::do_move_start(ssize_t sent) {
    start += sent;
}

void Buffer::add_data_to_end(const char * from, size_t size_data) {
    while (end + size_data > size) {
        do_resize(size * 2);
    }

    memcpy(buf + end, from, size_data);
    end += size_data;
}

void Buffer::add_symbol_to_end(char c) {
    while (end + 1 > size) {
        do_resize(size * 2);
    }

    buf[end++] = c;
}

Buffer::~Buffer() {
    fprintf(stderr, "Destructor buffer\n");
    fprintf(stderr, "Size: %ld\n", size);

    if (is_correct) {
        free(buf);
        perror("free");
    }
    else {
        fprintf(stderr, "Destructor buffer not correct\n");
    }

    fprintf(stderr, "Done buffer destructor\n");
}
