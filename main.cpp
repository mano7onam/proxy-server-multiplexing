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
#include <string>

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

struct Client {
    int my_socket;
    int http_socket;
    bool is_correct_my_socket;
    bool is_correct_http_socket;
    Buffer * buffer_in;
    Buffer * buffer_server_request;
    Buffer * buffer_out;
    bool is_received_get_request;
    int poll_id;

    Client(int my_socket, int size_buf) {
        this->my_socket = my_socket;
        is_correct_my_socket = true;
        is_correct_http_socket = true;
        buffer_in = new Buffer(size_buf);
        buffer_out = new Buffer(size_buf);
        buffer_server_request = new Buffer(size_buf);
        is_received_get_request = false;
    }

    ~Client() {
        fprintf(stderr, "Destructor client!!!!\n");
        if (is_correct_my_socket) {
            close(my_socket);
        }
        delete buffer_in;
        delete buffer_server_request;
        delete buffer_out;
    }
};

int server_socket;
std::vector<Client*> clients;

void init_server_socket(unsigned short server_port) {
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
}

void accept_incoming_connection() {
    struct sockaddr_in client_address;
    int address_size = sizeof(sockaddr_in);
    int client_socket = accept(server_socket, (struct sockaddr *)&client_address,
                               (socklen_t *)&address_size);

    if (client_socket <= 0) {
        perror("Error while accept()");
        exit(EXIT_FAILURE);
    }

    Client * new_actor = new Client(client_socket, DEFAULT_BUFFER_SIZE);
    clients.push_back(new_actor);
}

// two auxiliary vectors to delete closed on uncorrect clients
std::vector<bool> clients_to_delete;
std::vector<Client*> rest_clients;

std::pair<std::string, std::string> parse_hostname_and_path(char * uri) {
    char host_name[LITTLE_STRING_SIZE];
    char path[LITTLE_STRING_SIZE];

    char * protocolEnd = strstr(uri, "://");
    if (NULL == protocolEnd) {
        perror("Wrong protocol");
        return std::make_pair("", "");
    }

    char * host_end = strchr(protocolEnd + 3, '/');
    size_t host_length = 0;
    if (NULL == host_end) {
        host_length = strlen(protocolEnd + 3);
        path[0] = '/';
        path[1] = '\0';
    }
    else {
        host_length = host_end - (protocolEnd + 3);
        size_t path_size = strlen(uri) - (host_end - uri);
        strncpy(path, host_end, path_size);
        path[path_size] = '\0';
    }

    strncpy(host_name, protocolEnd + 3, host_length);
    host_name[host_length] = '\0';

    return std::make_pair(std::string(host_name), std::string(path));
}

// return host name and first line to server GET request
// "" - when any error in parsing
std::pair<std::string, std::string> parse_first_line_header(int i, char * p_new_line) {
    Buffer * client_buffer_in = clients[i]->buffer_in;

    if (client_buffer_in->size < 3 ||
        client_buffer_in->buf[0] != 'G' ||
        client_buffer_in->buf[1] != 'E' ||
        client_buffer_in->buf[2] != 'T')
    {
        fprintf(stderr, "Not GET request\n");
        return std::make_pair("", "");
    }

    char first_line[LITTLE_STRING_SIZE];
    size_t first_line_length = p_new_line - client_buffer_in->buf;
    strncpy(first_line, client_buffer_in->buf, first_line_length);
    first_line[first_line_length] = '\0';
    fprintf(stderr, "First line from client: %s\n", first_line);

    char * method = strtok(first_line, " ");
    char * uri = strtok(NULL, " ");
    char * version = strtok(NULL, "\n\0");

    fprintf(stderr, "method: %s\n", method);
    fprintf(stderr, "uri: %s\n", uri);
    fprintf(stderr, "version: %s\n", version);

    std::pair<std::string, std::string> parsed = parse_hostname_and_path(uri);
    std::string host_name = parsed.first;
    std::string path = parsed.second;
    if ("" == host_name || "" == path) {
        fprintf(stderr, "Hostname or path haven't been parsed\n");
        return std::make_pair("", "");
    }
    fprintf(stderr, "HostName: \'%s\'\n", host_name.c_str());
    fprintf(stderr, "Path: %s\n", path.c_str());

    std::string new_first_line = std::string(method) + " " + path + " " + std::string(version);

    return std::make_pair(host_name, new_first_line);
}

void receive_request_from_client(int i) {
    Buffer * client_buffer_in = clients[i]->buffer_in;
    ssize_t received = recv(clients[i]->my_socket, client_buffer_in->buf + client_buffer_in->end,
                            (size_t)(client_buffer_in->size - client_buffer_in->end), 0);

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
                int res1 = client_buffer_in->resize(new_size);
                int res2 = clients[i]->buffer_server_request->resize(new_size);
                if (-1 == res1 || -1 == res2) {
                    clients_to_delete[i] = true;
                    break;
                }
            }

            client_buffer_in->buf[client_buffer_in->end] = '\0';
            fprintf(stderr, "\nReceived from client: \n%s\n\n", client_buffer_in->buf);

            if (!clients[i]->is_received_get_request) {
                char * p_new_line = strchr(client_buffer_in->buf, '\n');
                if (p_new_line != NULL) {
                    clients[i]->is_received_get_request = true;

                    size_t i_next_line = p_new_line - client_buffer_in->buf + 1;
                    size_t size_without_first_line = client_buffer_in->end - i_next_line;

                    std::pair<std::string, std::string> parsed = parse_first_line_header(i, p_new_line);
                    std::string host_name = parsed.first;
                    std::string new_first_line = parsed.second;
                    if ("" == host_name || "" == new_first_line) {
                        clients_to_delete[i] = true;
                        break;
                    }
                    fprintf(stderr, "Hostname: %s\n", host_name.c_str());
                    fprintf(stderr, "New first line: %s\n", new_first_line.c_str());

                    memcpy(clients[i]->buffer_server_request->buf, new_first_line.c_str(), new_first_line.size());
                    clients[i]->buffer_server_request->end += new_first_line.size();
                    clients[i]->buffer_server_request->buf[clients[i]->buffer_server_request->end] = '\n';
                    clients[i]->buffer_server_request->end += 1;
                    memcpy(clients[i]->buffer_server_request->buf + clients[i]->buffer_server_request->end,
                           client_buffer_in->buf + i_next_line, size_without_first_line);
                    clients[i]->buffer_server_request->end += size_without_first_line;
                    clients[i]->buffer_server_request->buf[clients[i]->buffer_server_request->end] = '\0';
                    fprintf(stderr, "\n!!!!! New request: \n%s\n\n", clients[i]->buffer_server_request->buf);
                    client_buffer_in->start = client_buffer_in->end;

                    struct hostent * host_info = gethostbyname(host_name.c_str());
                    if (NULL == host_info) {
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
                    memcpy(&dest_addr.sin_addr, host_info->h_addr, host_info->h_length);

                    if (-1 == connect(http_socket,
                                      (struct sockaddr *)&dest_addr, sizeof(dest_addr)))
                    {
                        perror("Error while connect()");
                        clients_to_delete[i] = true;
                        break;
                    }

                    clients[i]->http_socket = http_socket;
                }
            }
            else {
                memcpy(clients[i]->buffer_server_request->buf + clients[i]->buffer_server_request->end,
                       client_buffer_in->buf + client_buffer_in->start,
                       (size_t)(client_buffer_in->end - client_buffer_in->start));
                clients[i]->buffer_server_request->end += (client_buffer_in->end - client_buffer_in->start);
                client_buffer_in->start = client_buffer_in->end;
            }
    }
}

void send_answer_to_client(int i) {
    bool flag_sent_answer_to_client = !clients_to_delete[i];
    while (flag_sent_answer_to_client &&
           clients[i]->is_received_get_request &&
           clients[i]->buffer_out->end > clients[i]->buffer_out->start)
    {
        fprintf(stderr, "\nHave data to send to client %d\n", i);
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

void delete_finished_clients() {
    fprintf(stderr, "\nClients size before clean: %ld\n", clients.size());
    rest_clients.clear();
    for (int i = 0; i < clients_to_delete.size(); ++i) {
        if (clients_to_delete[i]) {
            //fprintf(stderr, "%d - delete\n", i);
            delete clients[i];
        }
        else {
            //fprintf(stderr, "%d - rest, buf: %d, %d\n", i, clients[i]->buffer_in->buf, clients[i]->buffer_out->buf);
            rest_clients.push_back(clients[i]);
        }
    }
    clients = rest_clients;
    fprintf(stderr, "Clients size after clean: %ld\n", clients.size());
}

void receive_server_response(int i) {
    Buffer * client_buffer_out = clients[i]->buffer_out;
    ssize_t received = recv(clients[i]->http_socket, client_buffer_out->buf + client_buffer_out->end,
                            (size_t)(client_buffer_out->size - client_buffer_out->end), 0);

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
            client_buffer_out->buf[client_buffer_out->end] ='\0';
            fprintf(stderr, "\n\nReceived from http:\n%s\n\n", client_buffer_out->buf);
    }
}

void send_request_to_server(int i) {
    bool flag_send_request_to_server = !clients_to_delete[i];
    while ( flag_send_request_to_server &&
            clients[i]->is_received_get_request &&
            clients[i]->buffer_server_request->end > clients[i]->buffer_server_request->start) {
        fprintf(stderr, "\nHave data to send to http server (i):%d (fd):%d\n", i, clients[i]->http_socket);

        Buffer * client_buffer_in = clients[i]->buffer_server_request;
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
    init_server_socket(server_port);

    bool flag_execute = true;
    for ( ; flag_execute ; ) {
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
            perror("Error while select()");
            continue;
        }
        else if (0 == activity) {
            perror("poll() returned 0");
            continue;
        }

        if (FD_ISSET(server_socket, &fds)) {
            fprintf(stderr, "Have incoming client connection\n");
            accept_incoming_connection();
        }

        clients_to_delete.assign(clients.size(), false);
        for (int i = 0; i < clients.size(); ++i) {
            if (FD_ISSET(clients[i]->my_socket, &fds)) {
                fprintf(stderr, "Have data from client %d\n", i);
                receive_request_from_client(i);
                send_request_to_server(i);
            }
        }

        delete_finished_clients();

        clients_to_delete.assign(clients.size(), false);
        for (int i = 0; i < clients.size(); ++i) {
            if (!clients[i]->is_received_get_request) {
                continue;
            }

            if (clients[i]->is_received_get_request && FD_ISSET(clients[i]->http_socket, &fds)) {
                fprintf(stderr, "Have data (http response), (id):%d\n", i);
                receive_server_response(i);
                send_answer_to_client(i);
            }
        }

        delete_finished_clients();
    }

    for (auto client : clients) {
        delete client;
    }

    close(server_socket);

    return 0;
}
