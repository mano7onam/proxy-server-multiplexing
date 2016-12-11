#include "Includes.h"
#include "Client.h"

std::ofstream out_to_file("output.txt");

Cache * cache;

int my_server_socket;

std::vector<Client*> clients;

void init_my_server_socket(unsigned short server_port) {
    my_server_socket = socket(PF_INET, SOCK_STREAM, 0);

    int one = 1;
    setsockopt(my_server_socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    if (my_server_socket <= 0) {
        perror("Error while creating serverSocket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = PF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(server_port);

    if (bind(my_server_socket, (struct sockaddr *)&server_address, sizeof(server_address))) {
        perror("Error while binding");
        exit(EXIT_FAILURE);
    }

    if (listen(my_server_socket, 1024)) {
        perror("Error while listen()");
        exit(EXIT_FAILURE);
    }
}

void accept_incoming_connection() {
    struct sockaddr_in client_address;
    int address_size = sizeof(sockaddr_in);

    int client_socket = accept(my_server_socket, (struct sockaddr *)&client_address, (socklen_t *)&address_size);

    if (client_socket <= 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    Client * new_client = new Client(client_socket, cache);
    clients.push_back(new_client);
}

void push_data_to_request_from_cache(Client * client, std::pair<char*, size_t> data) {
    client->set_data_cached();

    Buffer * buffer_request = client->get_buffer_server_request();
    buffer_request->add_data_to_end(data.first, data.second);
}

int create_tcp_connection_to_request(Client * client, std::string host_name) {
    struct hostent * host_info = gethostbyname(host_name.c_str());

    if (NULL == host_info) {
        perror("gethostbyname");
        return RESULT_INCORRECT;
    }

    int http_socket = socket(PF_INET, SOCK_STREAM, 0);

    if (-1 == http_socket) {
        perror("socket");
        return RESULT_INCORRECT;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = PF_INET;
    dest_addr.sin_port = htons(DEFAULT_PORT);
    memcpy(&dest_addr.sin_addr, host_info->h_addr, host_info->h_length);

    /*int flags = fcntl(http_socket, F_GETFL, 0);
    fcntl(http_socket, F_SETFL, flags | O_NONBLOCK);*/

    fprintf(stderr, "Before connect\n");
    if (connect(http_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr))) {
        perror("connect");
        return RESULT_INCORRECT;
    }
    fprintf(stderr, "After connect\n");

    client->set_http_socket(http_socket);

    return RESULT_CORRECT;
}

int handle_first_line_proxy_request(Client * client, char * p_new_line, size_t i_next_line) {
    Buffer * buffer_in = client->get_buffer_in();

    std::pair<std::string, std::string> parsed = Parser::get_new_first_line_and_hostname(buffer_in, p_new_line);

    std::string host_name = parsed.first;
    std::string new_first_line = parsed.second;

    if ("" == host_name || "" == new_first_line) {
        fprintf(stderr, "Can not correctly parse first line\n");

        return RESULT_INCORRECT;
    }

    fprintf(stderr, "Hostname: %s\n", host_name.c_str());
    fprintf(stderr, "New first line: %s\n", new_first_line.c_str());

    if (false/* && cache->is_in_cache(parsed)*/) {
        Record record = cache->get_from_cache(parsed);
        push_data_to_request_from_cache(client, std::make_pair(record.data, record.size));

        return RESULT_CORRECT;
    }
    else {
        Parser::push_first_data_request(client->get_buffer_server_request(), buffer_in, new_first_line, i_next_line);
        int result_connection = create_tcp_connection_to_request(client, host_name);

        return result_connection;
    }
}

int move_end_client_in_buffer(Client * client, ssize_t received) {
    Buffer * client_buffer_in = client->get_buffer_in();
    client_buffer_in->do_move_end(received);
    return RESULT_CORRECT;
}

void delete_finished_clients() {
    //fprintf(stderr, "\nClients size before clean: %ld\n", clients.size());

    std::vector<Client*> rest_clients;

    for (auto client : clients) {
        if (client->is_closed()) {
            delete client;
        }
        else {
            int http_socket = client->get_http_socket();

            if (-1 != http_socket && client->is_closed_http_socket()) {
                fprintf(stderr, "Close http socket\n");
                if (close(http_socket)) {
                    perror("close");
                }
                client->set_http_socket(-1);
            }

            rest_clients.push_back(client);
        }
    }

    clients = rest_clients;

    //fprintf(stderr, "Clients size after clean: %ld\n", clients.size());
}

void receive_request_from_client(Client * client) {
    Buffer * buffer_in = client->get_buffer_in();
    ssize_t received = recv(client->get_my_socket(), buffer_in->get_end(), buffer_in->get_empty_space_size(), 0);

    fprintf(stderr, "[%d] Received: %ld\n", client->get_my_socket(), received);

    switch (received) {
        case -1:
            perror("Error while read()");
            client->set_closed_incorrect();
            break;

        case 0:
            fprintf(stderr, "Close client\n");
            client->set_closed_correct();
            break;

        default:
            int res = buffer_in->do_move_end(received);

            if (RESULT_INCORRECT == res) {
                client->set_closed_incorrect();
                break;
            }

            if (!client->is_received_get_request()) {
                char * p_new_line = strchr(buffer_in->get_start(), '\n');

                if (p_new_line != NULL) {
                    client->set_received_get_request();

                    size_t i_next_line = (p_new_line - buffer_in->get_start()) + 1;

                    int result = handle_first_line_proxy_request(client, p_new_line, i_next_line);

                    if (RESULT_INCORRECT == result) {
                        client->set_closed_incorrect();
                        break;
                    }
                }
            }
            else {
                Buffer * buffer_request = client->get_buffer_server_request();

                buffer_request->add_data_to_end(buffer_in->get_start(), buffer_in->get_data_size());
                buffer_in->do_move_start(buffer_in->get_data_size());
            }
    }
}

void send_answer_to_client(Client * client) {
    Buffer * buffer_out = client->get_buffer_out();

    if (!client->is_closed() && buffer_out->is_have_data()) {
        fprintf(stderr, "\nHave data to send to client\n");

        ssize_t sent = send(client->get_my_socket(), buffer_out->get_start(), buffer_out->get_data_size(), 0);

        fprintf(stderr, "[%d] Sent: %ld\n", client->get_my_socket(), sent);

        switch (sent) {
            case -1:
                perror("Error while send to client");
                client->set_closed_incorrect();
                break;

            case 0:
                client->set_closed_correct();
                break;

            default:
                buffer_out->do_move_start(sent);
        }
    }
}

void receive_server_response(Client * client) {
    fprintf(stderr, "Have data from server\n");

    Buffer * buffer_out = client->get_buffer_out();
    ssize_t received = recv(client->get_http_socket(), buffer_out->get_end(), buffer_out->get_empty_space_size(), 0);

    fprintf(stderr, "[%d] Received: %ldn\n", client->get_my_socket(), received);

    switch (received) {
        case -1:
            perror("recv(http)");
            client->set_closed_incorrect();
            break;

        case 0:
            fprintf(stderr, "Close http connection\n");
            fprintf(stderr, "Socket: %d\n", client->get_http_socket());

            client->set_close_http_socket();

            break;

        default:
            buffer_out->do_move_end(received);
    }
}

void send_request_to_server(Client * client) {
    Buffer * buffer_request = client->get_buffer_server_request();

    if (!client->is_data_cached() && buffer_request->is_have_data()) {
        fprintf(stderr, "\nHave data to send to http server\n");

        ssize_t sent = send(client->get_http_socket(), buffer_request->get_start(), buffer_request->get_data_size(), 0);

        fprintf(stderr, "[%d] Sent: %ld\n", client->get_my_socket(), sent);

        switch (sent) {
            case -1:
                perror("Error while send(http)");
                client->set_closed_incorrect();
                break;

            case 0:
                client->set_close_http_socket();
                break;

            default:
                buffer_request->do_move_start(sent);
        }
    }
}

void start_main_loop() {
    bool flag_execute = true;

    for ( ; flag_execute ; ) {
        fd_set fds_read;
        fd_set fds_write;
        FD_ZERO(&fds_read);
        FD_ZERO(&fds_write);
        int max_fd = 0;

        FD_SET(my_server_socket, &fds_read);
        max_fd = my_server_socket;

        for (auto client : clients) {
            FD_SET(client->get_my_socket(), &fds_read);

            if (client->get_buffer_out()->is_have_data()) {
                FD_SET(client->get_my_socket(), &fds_write);
            }

            max_fd = std::max(max_fd, client->get_my_socket());

            if (client->is_received_get_request() && -1 != client->get_http_socket() && !client->is_closed_http_socket()) {
                FD_SET(client->get_http_socket(), &fds_read);

                if (client->get_buffer_server_request()->is_have_data()) {
                    FD_SET(client->get_http_socket(), &fds_write);
                }

                max_fd = std::max(max_fd, client->get_http_socket());
            }
        }

        fprintf(stderr, "Before select\n");
        int activity = select(max_fd + 1, &fds_read, &fds_write, NULL, NULL);
        fprintf(stderr, "After select %d\n", activity);

        if (activity <= 0) {
            perror("select");
            continue;
        }

        for (auto client : clients) {
            int my_socket = client->get_my_socket();
            int http_socket = client->get_http_socket();
            fprintf(stderr, "Sockets: %d %d\n", my_socket, http_socket);
            fprintf(stderr, "My: %d %d\n", FD_ISSET(my_socket, &fds_read), FD_ISSET(my_socket, &fds_write));
            if (http_socket != -1) {
                fprintf(stderr, "Closed: %d\n", client->is_closed_http_socket());
                fprintf(stderr, "Http: %d %d\n", FD_ISSET(http_socket, &fds_read), FD_ISSET(http_socket, &fds_write));
            }
        }

        if (FD_ISSET(my_server_socket, &fds_read)) {
            fprintf(stderr, "Have incoming client connection\n");
            accept_incoming_connection();
        }

        for (auto client : clients) {
            //fprintf(stderr, "Current sockets: %d %d\n", client->get_my_socket(), client->get_http_socket());

            if (FD_ISSET(client->get_my_socket(), &fds_read)) {
                fprintf(stderr, "Have data from client\n");
                receive_request_from_client(client);
            }

            if (!client->is_data_cached() &&
                !client->is_closed_http_socket() &&
                FD_ISSET(client->get_http_socket(), &fds_write))
            {
                send_request_to_server(client);
            }

            if (!client->is_data_cached() &&
                    !client->is_closed_http_socket() &&
                    FD_ISSET(client->get_http_socket(), &fds_read))
            {
                fprintf(stderr, "%d\n", FD_ISSET(client->get_http_socket(), &fds_read));
                fprintf(stderr, "Have data (http response)\n");
                fprintf(stderr, "Socket: %d\n", client->get_http_socket());
                receive_server_response(client);
                fprintf(stderr, "Done\n");
            }

            if (FD_ISSET(client->get_my_socket(), &fds_write)) {
                send_answer_to_client(client);
            }
        }

        for (auto client : clients) {
            if (client->is_closed_http_socket() && !client->get_buffer_out()->is_have_data()) {
                client->set_closed_correct();
            }
        }

        delete_finished_clients();
    }
}

void signal_handle(int sig) {
    perror("perror");
    fprintf(stderr, "Exit with code %d\n", sig);

    delete cache;

    for (auto client : clients) {
        delete client;
    }

    fprintf(stderr, "Close my server socket\n");
    close(my_server_socket);

    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handle);
    signal(SIGKILL, signal_handle);
    signal(SIGTERM, signal_handle);
    signal(SIGSEGV, signal_handle);

    // todo GET_OPT!!!

    if (2 != argc) {
        perror("Wrong number of arguments");
        exit(EXIT_FAILURE);
    }

    unsigned short server_port = (unsigned short)atoi(argv[1]);
    if (0 == server_port) {
        perror("Wrong port for listening");
        exit(EXIT_FAILURE);
    }
    init_my_server_socket(server_port);

    start_main_loop();

    delete cache;

    // close clients
    for (auto client : clients) {
        delete client;
    }

    close(my_server_socket);

    return 0;
}
