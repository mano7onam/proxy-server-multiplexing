//
// Created by mano on 10.12.16.
//

#ifndef PORTFORWARDERREDEAWROFTROP_BUFFER_H
#define PORTFORWARDERREDEAWROFTROP_BUFFER_H

#include "Includes.h"

class Buffer {
    char * buf;

    size_t start;
    size_t end;
    size_t size;

    bool is_correct;

public:
    Buffer(size_t size);

    bool is_have_data() {
        return end - start > 0U;
    }

    size_t get_data_size() {
        return end - start;
    }

    size_t get_empty_space_size() {
        return size - end;
    }

    size_t get_capacity() {
        return size;
    }

    char * get_buf() {
        return buf;
    }

    size_t get_i_end() {
        return end;
    }

    size_t get_i_start() {
        return start;
    }

    char * get_start() {
        return buf + start;
    }

    char * get_end() {
        return buf + end;
    }

    int do_resize(size_t new_size);

    int do_move_end(ssize_t received);

    void do_move_start(ssize_t sent);

    void add_data_to_end(const char * from, size_t size_data);

    void add_symbol_to_end(char c);

    ~Buffer();
};


#endif //PORTFORWARDERREDEAWROFTROP_BUFFER_H
