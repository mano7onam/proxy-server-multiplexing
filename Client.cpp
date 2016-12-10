//
// Created by mano on 10.12.16.
//

#include "Client.h"

Client::Client(int my_socket, Cache * cache) {
    this->my_socket = my_socket;
    this->http_socket = -1;

    is_correct_my_socket = true;
    is_correct_http_socket = true;

    buffer_in = new Buffer(DEFAULT_BUFFER_SIZE);
    buffer_out = new Buffer(DEFAULT_BUFFER_SIZE);
    buffer_server_request = new Buffer(DEFAULT_BUFFER_SIZE);

    is_received_get_request = false;

    is_closed = false;

    is_data_cached = false;

    this->cache = cache;
}

void Client::add_result_to_cache() {
    if (!cache->is_in_cache(first_line_and_host)) {
        size_t size_data = buffer_out->end;
        char * data = (char*)malloc(size_data);
        memcpy(data, buffer_out->buf, size_data);
        m_cache[first_line_and_host] = std::make_pair(data, size_data);
    }
    
}
