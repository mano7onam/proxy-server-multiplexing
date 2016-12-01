#include <cstdlib> //exit, atoi
#include <cstdio> //perror
#include <netdb.h> //gethostbyname
#include <cstring> //memcpy
#include <sys/socket.h> //sockaddr_in, AF_INET, socket, bind, listen, connect
#include <poll.h> //poll
#include <unistd.h> //read, write, close
#include <arpa/inet.h> //htonl, htons
#include <vector>
#include <algorithm>

#define LITTLE_STRING_SIZE 4096
#define DEFAULT_BUFFER_SIZE (4096)
#define DEFAULT_PORT 80

struct Buffer {
    char * buf;
    int start;
    int end;
    int size;
    bool is_correct;

    Buffer(int size_buf) {
        buf = (char*)malloc((size_t)size_buf);
        if (NULL == buf) {
            perror("new buf");
            is_correct = false;
            exit(EXIT_FAILURE);
        }
        start = 0;
        end = 0;
        size = size_buf;
        is_correct = true;
    }

    int resize(int new_size_buf) {
        char * result = (char *)realloc(buf, (size_t)new_size_buf);
        if (NULL == result) {
            perror("realloc");
            free(buf);
            exit(EXIT_FAILURE);
        }
        buf = result;
        size = new_size_buf;
    }

    ~Buffer() {
        fprintf(stderr, "Destructor buffer!!!!\n");
        fprintf(stderr, "Buf: %d\n", buf);
        fprintf(stderr, "Size: %d\n", size);
        if (is_correct){
            free(buf);
            perror("free");
        }
        else {
            fprintf(stderr, "Destructor buffer not correct\n");
        }
        fprintf(stderr, "Done\n");
    }
};

struct Actor {
    int my_socket;
    int http_socket;
    bool is_correct_my_socket;
    bool is_correct_http_socket;
    Buffer * buffer_in;
    Buffer * buffer_out;
    bool is_received_get_request;
    int poll_id;

    Actor(int my_socket, int size_buf) {
        this->my_socket = my_socket;
        is_correct_my_socket = true;
        is_correct_http_socket = true;
        buffer_in = new Buffer(size_buf);
        buffer_out = new Buffer(size_buf);
        is_received_get_request = false;
    }

    ~Actor() {
        fprintf(stderr, "Destructor client!!!!\n");
        if (is_correct_my_socket) {
            close(my_socket);
        }
        delete buffer_in;
        delete buffer_out;
    }
};

std::vector<Actor*> clients;

int server_socket;

int main(int argc, char *argv[]) {
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

//======================================================================================

    server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == server_socket) {
        perror("Error while creating serverSocket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(server_port);

    if (-1 == (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)))) {
        perror("Error while binding");
        exit(EXIT_FAILURE);
    }

    if (-1 == listen(server_socket, 1024)) {
        perror("Error while listen()");
        exit(EXIT_FAILURE);
    }

//======================================================================================

    std::vector<bool> clients_to_delete;
    std::vector<Actor*> rest_clients;
    for ( ; ; ) {
        fd_set fds;
        FD_ZERO(&fds);
        int max_fd = 0;

        FD_SET(server_socket, &fds);
        max_fd = server_socket;

        for (auto client : clients) {
            FD_SET(client->my_socket, &fds);
            max_fd = std::max(max_fd, client->my_socket);
            if (client->is_received_get_request) {
                FD_SET(client->http_socket, &fds);
                max_fd = std::max(max_fd, client->http_socket);
            }
        }

        int activity = select(max_fd + 1, &fds, NULL, NULL, NULL);
        fprintf(stderr, "Activity: %d\n", activity);

        if (-1 == activity) {
            perror("Error while poll()");
            exit(EXIT_FAILURE);
        }
        else if (0 == activity) {
            perror("poll() returned 0");
            continue;
        }

        if (FD_ISSET(server_socket, &fds)) {
            fprintf(stderr, "Have incoming client connection\n");

            struct sockaddr_in client_address;
            int address_size = sizeof(sockaddr_in);
            int client_socket = accept(server_socket, (struct sockaddr *)&client_address,
                                       (socklen_t *)&address_size);

            if (client_socket <= 0) {
                perror("Error while accept()");
                exit(EXIT_FAILURE);
            }

            Actor * new_actor = new Actor(client_socket, DEFAULT_BUFFER_SIZE);
            clients.push_back(new_actor);
        }

        clients_to_delete.assign(clients.size(), false);
        for (int i = 0; i < clients.size(); ++i) {
            if (FD_ISSET(clients[i]->my_socket, &fds)) {
                fprintf(stderr, "Have data from client %d\n", i);

                Buffer * client_buffer_in = clients[i]->buffer_in;
                ssize_t received = recv(clients[i]->my_socket, client_buffer_in->buf + client_buffer_in->end,
                                        (size_t)(client_buffer_in->size - client_buffer_in->end), 0);
                fprintf(stderr, "Client[fd %d] receive: %ld\n", clients[i]->my_socket, received);

                switch (received) {
                    case -1:
                        perror("Error while read()");
                        clients[i]->is_correct_my_socket = false;
                        clients_to_delete[i] = true;
                        break;
                    case 0:
                        fprintf(stderr, "Close client\n");
                        clients_to_delete[i] = true;
                        break;
                    default:
                        client_buffer_in->end += received;
                        if (client_buffer_in->end == client_buffer_in->size) {
                            int new_size = client_buffer_in->size * 2;
                            int res = client_buffer_in->resize(new_size);
                            if (-1 == res) {
                                clients_to_delete[i] = true;
                                break;
                            }
                        }

                        fprintf(stderr, "Received from client: %s\n", client_buffer_in->buf);

                        if (!clients[i]->is_received_get_request) {
                            char * p_new_line = strchr(client_buffer_in->buf, '\n');
                            if (p_new_line != NULL) {
                                if (client_buffer_in->size < 3 ||
                                        client_buffer_in->buf[0] != 'G' ||
                                        client_buffer_in->buf[1] != 'E' ||
                                        client_buffer_in->buf[2] != 'T')
                                {
                                    fprintf(stderr, "Not GET request\n");
                                    clients_to_delete[i] = true;
                                    break;
                                }

                                clients[i]->is_received_get_request = true;
                                size_t first_line_length = p_new_line - client_buffer_in->buf;

                                char first_line[LITTLE_STRING_SIZE];
                                strncpy(first_line, client_buffer_in->buf, first_line_length);
                                first_line[first_line_length] = '\0';

                                fprintf(stderr, "Temp: %s\n", first_line);
                                char * method = strtok(first_line, " ");
                                char * uri = strtok(NULL, " ");
                                char * version = strtok(NULL, "\n\0");

                                fprintf(stderr, "method: %s\n", method);
                                fprintf(stderr, "uri: %s\n", uri);
                                fprintf(stderr, "version: %s\n", version);

                                char * protocolEnd = strstr(uri, "://");

                                if (NULL == protocolEnd) {
                                    perror("Wrong protocol");
                                    clients_to_delete[i] = true;
                                    break;
                                }
                                fprintf(stderr, "Good request parse\n");

                                char * host_end = strchr(protocolEnd + 3, '/');
                                size_t host_length = 0;
                                if (NULL == host_end) {
                                    host_length = strlen(protocolEnd + 3);
                                }
                                else {
                                    host_length = host_end - (protocolEnd + 3);
                                }

                                char host_name[LITTLE_STRING_SIZE];
                                strncpy(host_name, protocolEnd + 3, host_length);
                                host_name[host_length] = 0;

                                fprintf(stderr, "HostName: \'%s\'\n", host_name);

                                struct hostent * hostInfo = gethostbyname(host_name);
                                if (NULL == hostInfo) {
                                    perror("Error while gethostbyname");
                                    clients_to_delete[i] = true;
                                    break;
                                }

                                int http_socket = socket(AF_INET, SOCK_STREAM, 0);
                                if (-1 == http_socket) {
                                    perror("Error while socket()");
                                    exit(EXIT_FAILURE);
                                }

                                struct sockaddr_in dest_addr;
                                dest_addr.sin_family = AF_INET;
                                dest_addr.sin_port = htons(DEFAULT_PORT);
                                memcpy(&dest_addr.sin_addr, hostInfo->h_addr, hostInfo->h_length);

                                if (-1 == connect(http_socket,
                                                  (struct sockaddr *)&dest_addr, sizeof(dest_addr)))
                                {
                                    perror("Error while connect().\n");
                                    clients_to_delete[i] = true;
                                    break;
                                }

                                clients[i]->http_socket = http_socket;
                            }
                        }
                }
            }

            bool flag_sent_answer_to_client = !clients_to_delete[i];
            while (flag_sent_answer_to_client &&
                    clients[i]->is_received_get_request &&
                    clients[i]->buffer_out->end > clients[i]->buffer_out->start)
            {
                fprintf(stderr, "Have data to send to client %d\n", i);
                Buffer * client_buffer_out = clients[i]->buffer_out;
                ssize_t sent = send(clients[i]->my_socket, client_buffer_out->buf + client_buffer_out->start,
                                    (size_t)(client_buffer_out->end - client_buffer_out->start), 0);
                fprintf(stderr, "Sent: %ld\n", sent);
                switch (sent) {
                    case -1:
                        perror("Error while send to client");
                        clients[i]->is_correct_my_socket = false;
                        clients_to_delete[i] = true;
                        flag_sent_answer_to_client = false;
                        break;
                    case 0:
                        clients_to_delete[i] = true;
                        flag_sent_answer_to_client = false;
                        break;
                    default:
                        client_buffer_out->start += sent;
                }
            }
        }

        fprintf(stderr, "%ld\n", clients.size());
        rest_clients.clear();
        for (int i = 0; i < clients_to_delete.size(); ++i) {
            if (clients_to_delete[i]) {
                fprintf(stderr, "%d - delete\n", i);
                delete clients[i];
            }
            else {
                fprintf(stderr, "%d - rest, buf: %d, %d\n", i, clients[i]->buffer_in->buf, clients[i]->buffer_out->buf);
                rest_clients.push_back(clients[i]);
            }
        }
        clients = rest_clients;
        fprintf(stderr, "%ld\n", clients.size());

        clients_to_delete.assign(clients.size(), false);
        for (int i = 0; i < clients.size(); ++i) {
            if (!clients[i]->is_received_get_request) {
                continue;
            }

            if (clients[i]->is_received_get_request && FD_ISSET(clients[i]->http_socket, &fds)) {
                fprintf(stderr, "Have data (http response), (id):%d\n", i);

                Buffer * client_buffer_out = clients[i]->buffer_out;
                ssize_t received = recv(clients[i]->http_socket, client_buffer_out->buf + client_buffer_out->end,
                                        (size_t)(client_buffer_out->size - client_buffer_out->end), 0);
                fprintf(stderr, "Received from http: %ld %d\n", received, client_buffer_out->size - client_buffer_out->end);

                switch (received) {
                    case -1:
                        perror("recv(http)");
                        clients[i]->is_correct_http_socket = false;
                        clients_to_delete[i] = true;
                        break;
                    case 0:
                        fprintf(stderr, "Close http connection\n");
                        close(clients[i]->http_socket);
                        clients[i]->http_socket = -1;
                        break;
                    default:
                        client_buffer_out->end += received;
                        if (client_buffer_out->end == client_buffer_out->size) {
                            int new_size_buf = client_buffer_out->size * 2;
                            int res = client_buffer_out->resize(new_size_buf);
                            if (-1 == res) {
                                clients_to_delete[i] = true;
                                break;
                            }
                        }
                }
            }

            bool flag_send_request_to_server = !clients_to_delete[i];
            while ( flag_send_request_to_server &&
                    clients[i]->is_received_get_request &&
                    clients[i]->buffer_in->end > clients[i]->buffer_in->start) {
                fprintf(stderr, "Have data to send to http server (i):%d (fd):%d\n", i, clients[i]->http_socket);

                Buffer * client_buffer_in = clients[i]->buffer_in;
                ssize_t sent = send(clients[i]->http_socket, client_buffer_in->buf,
                                    (size_t)(client_buffer_in->end - client_buffer_in->start), 0);
                fprintf(stderr, "Sent to http: %ld, %d\n", sent, client_buffer_in->end - client_buffer_in->start);

                switch (sent) {
                    case -1:
                        perror("Error while send(http)");
                        clients[i]->is_correct_http_socket = false;
                        clients_to_delete[i] = true;
                        flag_send_request_to_server = false;
                        break;
                    case 0:
                        close(clients[i]->http_socket);
                        clients[i]->http_socket = -1;
                        flag_send_request_to_server = false;
                        break;
                    default:
                        client_buffer_in->start += sent;
                }
            }
        }

        rest_clients.clear();
        for (int i = 0; i < clients_to_delete.size(); ++i) {
            if (clients_to_delete[i]) {
                delete clients[i];
            }
            else {
                rest_clients.push_back(clients[i]);
            }
        }
        clients = rest_clients;
    }

    for (auto client : clients) {
        delete client;
    }

    close(server_socket);

    return 0;
}
