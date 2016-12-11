//
// Created by mano on 11.12.16.
//

#include "Parser.h"

std::pair<std::string, std::string> Parser::parse_hostname_and_path(char * uri) {
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

std::pair<std::string, std::string> Parser::get_new_first_line_and_hostname(Buffer * buffer_in, char *p_new_line) {
    if (buffer_in->get_data_size() < 3 ||
            buffer_in->get_start()[0] != 'G' ||
            buffer_in->get_start()[1] != 'E' ||
            buffer_in->get_start()[2] != 'T')
    {
        fprintf(stderr, "Not GET request\n");
        return std::make_pair("", "");
    }

    char first_line[LITTLE_STRING_SIZE];

    size_t first_line_length = p_new_line - buffer_in->get_start();
    strncpy(first_line, buffer_in->get_start(), first_line_length);

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

    std::string http10str = "HTTP/1.0";
    std::string new_first_line = std::string(method) + " " + path + " " + http10str;

    return std::make_pair(host_name, new_first_line);
}

void Parser::push_first_data_request(Buffer *buffer_request, Buffer *buffer_in, std::string first_line,
                                     size_t i_next_line)
{
    size_t size_without_first_line = (buffer_in->get_end() - buffer_in->get_start()) - i_next_line;

    buffer_request->add_data_to_end(first_line.c_str(), first_line.size());
    fprintf(stderr, "Size: %ld\n", buffer_request->get_data_size());
    buffer_request->add_symbol_to_end('\n');
    fprintf(stderr, "Size: %ld\n", buffer_request->get_data_size());
    buffer_request->add_data_to_end(buffer_in->get_start() + i_next_line, size_without_first_line);
    fprintf(stderr, "Size: %ld\n", buffer_request->get_data_size());

    buffer_request->get_end()[0] = '\0';
    fprintf(stderr, "\n!!!!! New request: \n%s", buffer_request->get_start());

    buffer_in->do_move_start(buffer_in->get_data_size());
    fprintf(stderr, "Done\n");
}
