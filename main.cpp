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
            //fprintf(stderr, "Sockets: %d %d\n", my_socket, http_socket);
            //fprintf(stderr, "My: %d %d\n", FD_ISSET(my_socket, &fds_read), FD_ISSET(my_socket, &fds_write));
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
                //receive_request_from_client(client);
                client->receive_request_from_client();
            }

            if (!client->is_data_cached() &&
                !client->is_closed_http_socket() &&
                FD_ISSET(client->get_http_socket(), &fds_write))
            {
                //send_request_to_server(client);
                client->send_request_to_server();
            }

            if (!client->is_data_cached() &&
                    !client->is_closed_http_socket() &&
                    FD_ISSET(client->get_http_socket(), &fds_read))
            {
                fprintf(stderr, "%d\n", FD_ISSET(client->get_http_socket(), &fds_read));
                fprintf(stderr, "Have data (http response)\n");
                fprintf(stderr, "Socket: %d\n", client->get_http_socket());

                //receive_server_response(client);
                client->receive_server_response();

                fprintf(stderr, "Done\n");
            }

            if (FD_ISSET(client->get_my_socket(), &fds_write)) {
                //send_answer_to_client(client);
                client->send_answer_to_client();
            }
        }

        for (auto client : clients) {
            if (client->is_closed_http_socket() && !client->get_buffer_out()->is_have_data()) {
                client->set_closed_correct();
            }
            if (client->is_data_cached() && !client->get_buffer_out()->is_have_data()) {
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

    cache = new Cache();

    start_main_loop();

    delete cache;

    // close clients
    for (auto client : clients) {
        delete client;
    }

    close(my_server_socket);

    return 0;
}
