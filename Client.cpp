//
// Created by mano on 10.12.16.
//

#include "Client.h"
#include "Parser.h"

Client::Client(int my_socket, Cache * cache) {
    this->my_socket = my_socket;
    this->http_socket = -1;

    flag_correct_my_socket = true;
    flag_correct_http_socket = true;

    buffer_in = new Buffer(DEFAULT_BUFFER_SIZE);
    buffer_in_offs = 0U;

    buffer_server_request = NULL;
    buffer_server_request_offs = 0U;

    buffer_out = NULL;
    buffer_out_offs = 0U;

    flag_received_get_request = false;

    flag_closed = false;
    flag_closed_http_socket = false;
    flag_closed_my_socket = false;

    flag_process_http_connecting = false;

    flag_take_data_from_cache = false;

    this->cache = cache;
}

int Client::create_tcp_connection_to_request(std::string host_name) {
    struct hostent * host_info = gethostbyname(host_name.c_str());

    if (NULL == host_info) {
        perror("gethostbyname");
        return RESULT_INCORRECT;
    }

    http_socket = socket(PF_INET, SOCK_STREAM, 0);

    if (-1 == http_socket) {
        perror("socket");
        return RESULT_INCORRECT;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = PF_INET;
    dest_addr.sin_port = htons(DEFAULT_PORT);
    memcpy(&dest_addr.sin_addr, host_info->h_addr, host_info->h_length);

    http_socket_flags = fcntl(http_socket, F_GETFL, 0);
    fcntl(http_socket, F_SETFL, http_socket_flags | O_NONBLOCK);

    fprintf(stderr, "Connect to http server\n");

    if (connect(http_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr))) {
        if (errno == EINPROGRESS) {
            fprintf(stderr, "Connect in progress\n");
            flag_process_http_connecting = true;
            return RESULT_CORRECT;
        }
        else {
            perror("connect");
            return RESULT_INCORRECT;
        }
    }

    flag_process_http_connecting = false;

    fprintf(stderr, "Connection established\n");

    return RESULT_CORRECT;
}

int Client::handle_first_line_proxy_request(char *p_new_line, size_t i_next_line) {
    std::pair<std::string, std::string> parsed = Parser::get_new_first_line_and_hostname(buffer_in, p_new_line);

    std::string host_name = parsed.first;
    std::string new_first_line = parsed.second;

    if ("" == host_name || "" == new_first_line) {
        fprintf(stderr, "Can not correctly parse first line\n");
        return RESULT_INCORRECT;
    }

    fprintf(stderr, "Hostname: %s\n", host_name.c_str());
    fprintf(stderr, "New first line: %s\n", new_first_line.c_str());

    flag_received_get_request = true;

    if (cache->is_in_cache(parsed)) {
        buffer_out = cache->get_from_cache(parsed);
        flag_take_data_from_cache = true;

        return RESULT_CORRECT;
    }
    else {
        buffer_server_request = new Buffer(DEFAULT_BUFFER_SIZE);
        buffer_out = new Buffer(DEFAULT_BUFFER_SIZE);

        Parser::push_first_data_request(buffer_server_request, buffer_in, new_first_line, i_next_line);
        buffer_in_offs = buffer_in->get_data_size(0U);

        cache_key = parsed;
        cache->push_to_cache(cache_key, buffer_out);

        int result_connection = create_tcp_connection_to_request(host_name);

        return result_connection;
    }
}

void Client::receive_request_from_client() {
    ssize_t received = recv(my_socket, buffer_in->get_end(), buffer_in->get_empty_space_size(), 0);

    fprintf(stderr, "My socket: [%d], Received: %ld\n", my_socket, received);

    switch (received) {
        case -1:
            perror("Error while read()");
            set_closed_incorrect();
            break;

        case 0:
            fprintf(stderr, "Close client\n");
            set_close_my_socket();
            break;

        default:
            int res = buffer_in->do_move_end(received);

            if (RESULT_INCORRECT == res) {
                set_closed_incorrect();
                break;
            }

            if (!flag_received_get_request) {
                char * p_new_line = strchr(buffer_in->get_buf_offs(0U), '\n');

                if (p_new_line != NULL) {
                    flag_received_get_request = true;

                    size_t i_next_line = (p_new_line - buffer_in->get_buf_offs(0U)) + 1;
                    int result = handle_first_line_proxy_request(p_new_line, i_next_line);

                    if (RESULT_INCORRECT == result) {
                        set_closed_incorrect();
                        break;
                    }
                }
            }
            else {
                char * from = buffer_in->get_buf_offs(buffer_in_offs);
                size_t data_size = buffer_in->get_data_size(buffer_in_offs);
                buffer_server_request->add_data_to_end(from, data_size);
                buffer_in_offs += data_size;
            }
    }
}

void Client::send_answer_to_client() {
    fprintf(stderr, "\nHave data to send to client\n");

    char * data = buffer_out->get_buf_offs(buffer_out_offs);
    size_t data_size = buffer_out->get_data_size(buffer_out_offs);
    ssize_t sent = send(my_socket, data, data_size, 0);

    fprintf(stderr, "[%d] Sent: %ld\n", my_socket, sent);

    switch (sent) {
        case -1:
            perror("Error while send to client");
            set_close_my_socket();
            break;

        case 0:
            set_close_my_socket();
            break;

        default:
            buffer_out_offs += sent;
    }
}

void Client::receive_server_response() {
    fprintf(stderr, "Have data from server\n");

    if (flag_process_http_connecting) {
        fprintf(stderr, "Http connection established\n");

        fcntl(http_socket, F_SETFL, http_socket_flags);
        flag_process_http_connecting = false;

        return;
    }

    ssize_t received = recv(http_socket, buffer_out->get_end(), buffer_out->get_empty_space_size(), 0);

    fprintf(stderr, "My socket: [%d], Received: %ldn\n", my_socket, received);

    switch (received) {
        case -1:
            perror("recv(http)");
            set_closed_incorrect();
            break;

        case 0:
            fprintf(stderr, "Set closed http connection\n");
            set_close_http_socket();
            break;

        default:
            buffer_out->do_move_end(received);
    }
}

void Client::send_request_to_server() {
    fprintf(stderr, "Send data to server\n");

    char * data = buffer_server_request->get_buf_offs(buffer_server_request_offs);
    size_t data_size = buffer_server_request->get_data_size(buffer_server_request_offs);
    ssize_t sent = send(http_socket, data, data_size, 0);

    fprintf(stderr, "My socket: [%d], Sent: %ld\n", my_socket, sent);

    switch (sent) {
        case -1:
            perror("Error while send(http)");
            set_closed_incorrect();
            break;

        case 0:
            set_close_http_socket();
            break;

        default:
            buffer_server_request_offs += sent;
    }
}

bool Client::can_recv_from_client() {
    return !is_closed_my_socket();
}

bool Client::can_recv_from_server() {
    return !flag_take_data_from_cache && !is_closed_http_socket();
}

bool Client::is_have_data_for_client() {
    if (NULL == buffer_out || is_closed_my_socket()) {
        return false;
    }

    return buffer_out->is_have_data(buffer_out_offs);
}

bool Client::is_have_data_for_server() {
    if (flag_take_data_from_cache || NULL == buffer_server_request || is_closed_http_socket()) {
        return false;
    }

    return buffer_server_request->is_have_data(buffer_server_request_offs);
}

bool Client::can_be_closed() {
    if (is_closed_my_socket() && (is_closed_http_socket() || !is_received_get_request())) {
        return true;
    }

    if (NULL == buffer_out) {
        return false;
    }

    if (flag_take_data_from_cache) {
        return buffer_out->is_finished() && !buffer_out->is_have_data(buffer_out_offs);
    }
    else {
        return is_closed_http_socket() && !buffer_out->is_have_data(buffer_out_offs);
    }
}

Client::~Client() {
    fprintf(stderr, "Destructor client\n");

    if (-1 != my_socket && flag_correct_my_socket) {
        close(my_socket);
    }

    if (-1 != http_socket && flag_correct_http_socket) {
        close(http_socket);
    }

    if (!flag_take_data_from_cache) {
        if (NULL == buffer_out || !flag_closed_correct) {
            cache->delete_from_cache(cache_key);
        }
        else {
            buffer_out->set_finished_correct();
        }
    }

    if (NULL != buffer_in) {
        delete buffer_in;
    }

    if (NULL != buffer_server_request) {
        delete buffer_server_request;
    }

    fprintf(stderr, "Destructor client done\n");
}
