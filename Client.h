//
// Created by mano on 10.12.16.
//

#ifndef LITTLEPROXYSERVER_CLIENT_H
#define LITTLEPROXYSERVER_CLIENT_H


class Client {
    int my_socket;
    int http_socket;

    bool is_correct_my_socket;
    bool is_correct_http_socket;

    Buffer * buffer_in;
    Buffer * buffer_server_request;
    Buffer * buffer_out;

    bool is_received_get_request;

    bool is_closed;
    bool is_closed_correct;

    Cache * cache;

    bool is_data_cached;

    struct sockaddr_in dest_addr;
    long long last_time_activity;

    std::pair<std::string, std::string> first_line_and_host;

public:

    Client(int my_socket, size_t size_buf);

    void add_result_to_cache();

    void set_closed_correct();

    void set_closed_incorrect();

    ~Client();

};


#endif //LITTLEPROXYSERVER_CLIENT_H
