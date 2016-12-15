//
// Created by mano on 10.12.16.
//

#ifndef PORTFORWARDERREDEAWROFTROP_CACHE_H
#define PORTFORWARDERREDEAWROFTROP_CACHE_H

#include "Includes.h"
#include "Buffer.h"

class Cache {
    std::map<std::pair<std::string, std::string>, Buffer*> cache;

public:

    Cache();

    bool is_in_cache(std::pair<std::string, std::string> key);

    Buffer * get_from_cache(std::pair<std::string, std::string> key);

    int push_to_cache(std::pair<std::string, std::string> key, Buffer * data);

    void delete_from_cache(std::pair<std::string, std::string> key);

    ~Cache();
};


#endif //PORTFORWARDERREDEAWROFTROP_CACHE_H
