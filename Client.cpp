//
// Created by mano on 10.12.16.
//

#include "Client.h"

Client::Client(int my_socket, Cache * cache) {
    this->my_socket = my_socket;
    this->http_socket = -1;

    flag_correct_my_socket = true;
    flag_correct_http_socket = true;

    buffer_in = new Buffer(DEFAULT_BUFFER_SIZE);
    buffer_out = new Buffer(DEFAULT_BUFFER_SIZE);
    buffer_server_request = new Buffer(DEFAULT_BUFFER_SIZE);

    flag_received_get_request = false;

    flag_closed = false;
    flag_closed_http_socket = false;

    flag_data_cached = false;

    this->cache = cache;
}

void Client::add_result_to_cache() {
    if (!cache->is_in_cache(first_line_and_host)) {
        size_t size_data = buffer_out->get_i_end();

        char * data = (char*)malloc(size_data);
        memcpy(data, buffer_out->get_buf(), size_data);

        cache->push_to_cache(first_line_and_host, data, size_data);
    }
}

Client::~Client() {
    fprintf(stderr, "Destructor client!!!!\n");

    if (-1 != my_socket && flag_correct_my_socket) {
        close(my_socket);
    }

    if (-1 != http_socket && flag_correct_http_socket) {
        close(http_socket);
    }

    /*if (flag_closed_correct) {
        add_result_to_cache();
    }*/

    delete buffer_in;
    delete buffer_server_request;
    delete buffer_out;
}
