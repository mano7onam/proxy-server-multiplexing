//
// Created by mano on 10.12.16.
//

#ifndef LITTLEPROXYSERVER_CLIENT_H
#define LITTLEPROXYSERVER_CLIENT_H

#include "Cache.h"
#include "Buffer.h"
#include "Parser.h"

class Client {
    int my_socket;
    int http_socket;

    bool flag_correct_my_socket;
    bool flag_correct_http_socket;

    Buffer * buffer_in;
    Buffer * buffer_server_request;
    Buffer * buffer_out;

    bool flag_received_get_request;

    bool flag_closed;
    bool flag_closed_correct;
    bool flag_closed_http_socket;
    bool flag_process_http_connecting;

    Cache * cache;

    bool flag_data_cached;

    struct sockaddr_in dest_addr;
    long long last_time_activity;

    std::pair<std::string, std::string> first_line_and_host;

public:

    Client(int my_socket, Cache * cache);

    void add_result_to_cache();

    int get_my_socket() {
        return my_socket;
    }

    int get_http_socket() {
        return http_socket;
    }

    void set_data_cached() {
        flag_data_cached = true;
    }

    bool is_data_cached() {
        return flag_data_cached;
    }

    void set_closed_correct() {
        flag_closed = true;
        flag_closed_correct = true;
    }

    void set_closed_incorrect() {
        flag_closed = true;
        flag_closed_correct = false;
    }

    bool is_closed() {
        return flag_closed;
    }

    void set_close_http_socket() {
        flag_closed_http_socket = true;
    }

    bool is_closed_http_socket() {
        return flag_closed_http_socket;
    }

    void set_http_socket(int http_socket) {
        this->http_socket = http_socket;
    }

    void set_received_get_request() {
        flag_received_get_request = true;
    }

    bool is_received_get_request() {
        return flag_received_get_request;
    }

    void set_process_http_connecting() {
        flag_process_http_connecting = true;
    }

    void set_http_connected() {
        flag_process_http_connecting = false;
    }

    bool is_process_http_connecting() {
        return flag_process_http_connecting;
    }

    Buffer * get_buffer_server_request() {
        return buffer_server_request;
    }

    Buffer * get_buffer_in() {
        return buffer_in;
    }

    Buffer * get_buffer_out() {
        return buffer_out;
    }



    ~Client();

};


#endif //LITTLEPROXYSERVER_CLIENT_H
