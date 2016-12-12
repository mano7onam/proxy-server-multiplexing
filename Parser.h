//
// Created by mano on 11.12.16.
//

#ifndef LITTLEPROXYSERVER_PARSER_H
#define LITTLEPROXYSERVER_PARSER_H

#include "Includes.h"
#include "Buffer.h"

class Parser {
public:

    static std::pair<std::string, std::string> parse_hostname_and_path(char * uri);

    static std::pair<std::string, std::string> get_new_first_line_and_hostname(Buffer * buffer_in, char * p_new_line);

    static void push_first_data_request(Buffer * buffer_request, Buffer * buffer_in, std::string first_line,
                                        size_t i_next_line);

    static void print_buffer_data(char * data, size_t size);
};


#endif //LITTLEPROXYSERVER_PARSER_H
