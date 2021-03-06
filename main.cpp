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

            if (client->is_have_data_for_client()) {
                FD_SET(client->get_my_socket(), &fds_write);
            }

            max_fd = std::max(max_fd, client->get_my_socket());

            if (client->is_received_get_request() && -1 != client->get_http_socket() && !client->is_closed_http_socket()) {
                FD_SET(client->get_http_socket(), &fds_read);

                if (client->is_have_data_for_server()) {
                    FD_SET(client->get_http_socket(), &fds_write);
                }

                max_fd = std::max(max_fd, client->get_http_socket());
            }
        }

        int activity = select(max_fd + 1, &fds_read, &fds_write, NULL, NULL);

        if (activity <= 0) {
            perror("select");
            continue;
        }

        if (FD_ISSET(my_server_socket, &fds_read)) {
            fprintf(stderr, "Have incoming client connection\n");
            accept_incoming_connection();
        }

        for (auto client : clients) {
            if (client->can_recv_from_client() && FD_ISSET(client->get_my_socket(), &fds_read)) {
                fprintf(stderr, "Have data from client\n");
                client->receive_request_from_client();
            }

            if (client->is_have_data_for_server() && FD_ISSET(client->get_http_socket(), &fds_write)) {
                client->send_request_to_server();
            }

            if (client->can_recv_from_server() && FD_ISSET(client->get_http_socket(), &fds_read)) {
                client->receive_server_response();
            }

            if (client->is_have_data_for_client() && FD_ISSET(client->get_my_socket(), &fds_write)) {
                client->send_answer_to_client();
            }
        }

        for (auto client : clients) {
            if (client->can_be_closed()) {
                fprintf(stderr, "Can to close client\n");
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

    char bad_request[100] = "Bad request\n\0";

    start_main_loop();

    delete cache;

    // close clients
    for (auto client : clients) {
        delete client;
    }

    close(my_server_socket);

    return 0;
}
