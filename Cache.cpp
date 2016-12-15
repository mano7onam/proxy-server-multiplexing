//
// Created by mano on 10.12.16.
//

#include "Cache.h"
#include <iostream>
#include <fstream>

Cache::Cache() {

}

bool Cache::is_in_cache(std::pair<std::string, std::string> key) {
    fprintf(stderr, "Check in cache: %s %s\n", key.first.c_str(), key.second.c_str());

    bool result = (bool) cache.count(key);
    return result;
}

Buffer * Cache::get_from_cache(std::pair<std::string, std::string> key) {
    fprintf(stderr, "Get from cache: %s %s\n", key.first.c_str(), key.second.c_str());

    if (!cache.count(key)) {
        return NULL;
    }
    else {
        return cache[key];
    }
}

void Cache::delete_from_cache(std::pair<std::string, std::string> key) {
    if (!cache.count(key)) {
        return;
    }

    if (cache[key] != NULL) {
        delete cache[key];
    }

    cache.erase(key);
}

int Cache::push_to_cache(std::pair<std::string, std::string> key, Buffer * data) {
    fprintf(stderr, "Push to cache: %s %s\n", key.first.c_str(), key.second.c_str());

    if (cache.count(key)) {
        delete cache[key];
    }

    cache[key] = data;
}

Cache::~Cache() {
    for (auto elem : cache) {
        delete elem.second;
    }
}