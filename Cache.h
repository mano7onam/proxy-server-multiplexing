//
// Created by mano on 10.12.16.
//

#ifndef PORTFORWARDERREDEAWROFTROP_CACHE_H
#define PORTFORWARDERREDEAWROFTROP_CACHE_H

#include "Includes.h"

struct Record {
    char * data;
    size_t size;
    long long last_time_use;
};

class Cache {
    std::map<std::pair<std::string, std::string>, Record> cache;

public:

    Cache();

    bool is_in_cache(std::pair<std::string, std::string> key);

    Record get_from_cache(std::pair<std::string, std::string> key);

    int push_to_cache(std::pair<std::string, std::string> key, char * value, size_t size_value);

    void delete_from_cache(std::pair<std::string, std::string> key);

    ~Cache();
};


#endif //PORTFORWARDERREDEAWROFTROP_CACHE_H
