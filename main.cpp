#include <cstdlib> //exit, atoi
#include <cstdio> //perror
#include <netdb.h> //gethostbyname
#include <cstring> //memcpy
#include <sys/socket.h> //sockaddr_in, AF_INET, socket, bind, listen, connect
#include <poll.h> //poll
#include <unistd.h> //read, write, close
#include <arpa/inet.h> //htonl, htons
#include <vector>

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
        start = 0;
        end = 0;
        size = size_buf;
        is_correct = true;
    }

    int resize(int new_size_buf) {
        char * result = (char *)realloc(buf, (size_t)size);
        if (NULL == result) {
            perror("Error while incoming realloc");
            free(buf);
            is_correct = false;
            return -1;
        }
        else {
            buf = result;
            return 1;
        }
    }

    ~Buffer() {
        if (is_correct){
            free(buf);
        }
    }
};

struct Actor {
    struct pollfd my_pollfd;
    struct pollfd http_pollfd;
    bool is_correct_my_socket;
    bool is_correct_http_socket;
    Buffer * buffer_in;
    Buffer * buffer_out;
    bool is_received_get_request;
    int poll_id;

    Actor(struct pollfd my_pollfd, int size_buf) {
        this->my_pollfd = my_pollfd;
        is_correct_my_socket = true;
        is_correct_http_socket = true;
        buffer_in = new Buffer(size_buf);
        buffer_out = new Buffer(size_buf);
        is_received_get_request = false;
    }

    ~Actor() {
        if (is_correct_my_socket) {
            close(my_pollfd.fd);
        }
        delete buffer_in;
        delete buffer_out;
    }
};

std::vector<Actor*> clients;

int server_socket;
struct pollfd server_pollfd;

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

    server_pollfd.fd = server_socket;
    server_pollfd.events = POLLIN;

//======================================================================================

    for ( ; ; ) {
        fprintf(stderr, "Before poll\n");
        struct pollfd * sockets = new struct pollfd[1 + 2 * clients.size()];
        int it_sockets = 0;
        sockets[0] = server_pollfd;
        ++it_sockets;
        for (auto client : clients) {
            sockets[it_sockets] = client->my_pollfd;
            ++it_sockets;
            if (client->is_received_get_request) {
                sockets[it_sockets] = client->http_pollfd;
                ++it_sockets;
            }
        }
        fprintf(stderr, "Size into: %d\n", it_sockets);
        int ready_count = poll(sockets, (size_t)it_sockets, -1);
        fprintf(stderr, "After poll: %d\n", ready_count);

        server_pollfd = sockets[0];
        it_sockets = 1;
        for (auto client : clients) {
            client->my_pollfd = sockets[it_sockets];
            ++it_sockets;
            if (client->is_received_get_request) {
                client->http_pollfd = sockets[it_sockets];
                ++it_sockets;
            }
        }

        if (-1 == ready_count) {
            perror("Error while poll()");
            exit(EXIT_FAILURE);
        }
        else if (0 == ready_count) {
            perror("poll() returned 0");
            delete[] sockets;
            continue;
        }

        if (0 != (server_pollfd.revents & POLLIN)) {
            fprintf(stderr, "Have incoming client connection\n");

            struct sockaddr_in client_address;
            int address_size = sizeof(sockaddr_in);
            int client_socket = accept(server_socket, (struct sockaddr *)&client_address,
                                       (socklen_t *)&address_size);

            if (client_socket <= 0) {
                perror("Error while accept()");
                exit(EXIT_FAILURE);
            }

            struct pollfd new_client_pollfd;
            new_client_pollfd.fd = client_socket;
            new_client_pollfd.events = POLLIN;
            clients.push_back(new Actor(new_client_pollfd, DEFAULT_BUFFER_SIZE));

            ready_count--;
        }

        std::vector<int> clients_to_delete;
        for (int i = 0; i < clients.size() && ready_count > 0; ++i) {
            struct pollfd client_pollfd = clients[i]->my_pollfd;
            if (0 != (client_pollfd.revents & POLLIN)) {
                fprintf(stderr, "Have data from client %d\n", i);

                Buffer * client_buffer_in = clients[i]->buffer_in;
                ssize_t received = recv(client_pollfd.fd, client_buffer_in->buf + client_buffer_in->end,
                                        (size_t)(client_buffer_in->size - client_buffer_in->end), 0);
                fprintf(stderr, "Client[fd %d] receive: %ld\n", client_pollfd.fd, received);

                switch (received) {
                    case -1:
                        perror("Error while read()");
                        clients[i]->is_correct_my_socket = false;
                        clients_to_delete.push_back(i);
                        break;
                    case 0:
                        fprintf(stderr, "Close client");
                        clients_to_delete.push_back(i);
                        break;
                    default:
                        client_buffer_in->end += received;
                        if (client_buffer_in->end == client_buffer_in->size) {
                            int new_size = client_buffer_in->size * 2;
                            int res = client_buffer_in->resize(new_size);
                            if (-1 == res) {
                                clients_to_delete.push_back(i);
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
                                    clients_to_delete.push_back(i);
                                    break;
                                }

                                clients[i]->is_received_get_request = true;
                                size_t first_line_length = p_new_line - client_buffer_in->buf;

                                char * first_line = new char[first_line_length + 1];
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
                                    clients_to_delete.push_back(i);
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

                                char * host_name = new char[host_length + 1];
                                strncpy(host_name, protocolEnd + 3, host_length);
                                host_name[host_length] = 0;

                                fprintf(stderr, "HostName: \'%s\'\n", host_name);

                                struct hostent * hostInfo = gethostbyname(host_name);
                                if (NULL == hostInfo) {
                                    perror("Error while gethostbyname");
                                    clients_to_delete.push_back(i);
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
                                    clients_to_delete.push_back(i);
                                    break;
                                }

                                struct pollfd http_pollfd;
                                http_pollfd.fd = http_socket;
                                http_pollfd.events = POLLOUT;
                                clients[i]->http_pollfd = http_pollfd;

                                //delete[] host_name;
                                delete[] first_line;
                            }
                        }
                }
                ready_count--;
            }
            else if (clients[i]->is_received_get_request && /*0 != (client_pollfd.fd & POLLOUT) &&*/
                    clients[i]->buffer_out->end > clients[i]->buffer_out->start) {
                fprintf(stderr, "Have data to send to client %d\n", i);
                Buffer * client_buffer_out = clients[i]->buffer_out;
                ssize_t sent = send(client_pollfd.fd, client_buffer_out->buf + client_buffer_out->start,
                                    (size_t)(client_buffer_out->end - client_buffer_out->start), 0);
                fprintf(stderr, "Sent: %ld\n", sent);
                switch (sent) {
                    case -1:
                        perror("Error while send to client");
                        clients[i]->is_correct_my_socket = false;
                        clients_to_delete.push_back(i);
                    case 0:
                        clients_to_delete.push_back(i);
                        break;
                    default:
                        client_buffer_out->start += sent;
                        if (client_buffer_out->start == client_buffer_out->end) {
                            clients[i]->my_pollfd.events &= ~POLLOUT;
                        }
                }
            }
        }

        for (int i = 0; i < clients_to_delete.size(); ++i) {
            delete clients[clients_to_delete[i]];
            clients.erase(clients.begin() + clients_to_delete[i]);
        }
        clients_to_delete.clear();

        for (int i = 0; i < clients.size(); ++i) {
            if (!clients[i]->is_received_get_request) {
                continue;
            }

            struct pollfd http_pollfd = clients[i]->http_pollfd;
            if (-1 != http_pollfd.fd/* && 0 != (http_pollfd.fd & POLLOUT)
                    clients[i]->buffer_in->end > clients[i]->buffer_in->start*/) {
                fprintf(stderr, "Have data to send to http server (i):%d (fd):%d\n", i, http_pollfd.fd);

                Buffer * client_buffer_in = clients[i]->buffer_in;
                ssize_t sent = send(http_pollfd.fd, client_buffer_in->buf,
                                    (size_t)(client_buffer_in->end - client_buffer_in->start), 0);
                fprintf(stderr, "Sent to http: %ld, %d\n", sent, client_buffer_in->end - client_buffer_in->start);

                switch (sent) {
                    case -1:
                        perror("Error while send(http)");
                        clients[i]->is_correct_http_socket = false;
                        clients_to_delete.push_back(i);
                        break;
                    case 0:
                        close(http_pollfd.fd);
                        clients[i]->http_pollfd.fd = -1;
                        break;
                    default:
                        client_buffer_in->start += sent;
                        if (client_buffer_in->start == client_buffer_in->end) {
                            clients[i]->http_pollfd.events &= ~POLLOUT;
                        }
                }
            }
            if (true || -1 != http_pollfd.fd && 0 != (http_pollfd.fd & POLLIN)) {
                fprintf(stderr, "Have data - http response (id):%d\n", i);

                Buffer * client_buffer_out = clients[i]->buffer_out;
                ssize_t received = recv(http_pollfd.fd, client_buffer_out->buf + client_buffer_out->end,
                                        (size_t)(client_buffer_out->size - client_buffer_out->end), 0);
                fprintf(stderr, "Received from http: %ld %d\n", received, client_buffer_out->size - client_buffer_out->end);

                switch (received) {
                    case -1:
                        perror("recv(http)");
                        clients[i]->is_correct_http_socket = false;
                        clients_to_delete.push_back(i);
                        break;
                    case 0:
                        fprintf(stderr, "Close http connection\n");
                        close(http_pollfd.fd);
                        http_pollfd.fd = -1;
                        clients[i]->http_pollfd.fd = -1;
                        break;
                    default:
                        client_buffer_out->end += received;
                        if (client_buffer_out->end == client_buffer_out->size) {
                            int new_size_buf = client_buffer_out->size * 2;
                            int res = client_buffer_out->resize(new_size_buf);
                            if (-1 == res) {
                                clients_to_delete.push_back(i);
                                break;
                            }
                        }
                        clients[i]->my_pollfd.events |= POLLOUT;
                }
            }

        }

        for (int i = 0; i < clients_to_delete.size(); ++i) {
            delete clients[clients_to_delete[i]];
            clients.erase(clients.begin() + clients_to_delete[i]);
            fprintf(stderr, "Delete client: %d\n", i);
        }
        clients_to_delete.clear();

        delete[] sockets;
    }

    for (auto client : clients) {
        delete client;
    }

    close(server_socket);

    return 0;
}
