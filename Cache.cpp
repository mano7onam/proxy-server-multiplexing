//
// Created by mano on 10.12.16.
//

#include "Cache.h"
#include <iostream>
#include <fstream>

Cache::Cache() {

}

bool Cache::is_in_cache(std::pair<std::string, std::string> key) {
    fprintf(stderr, "is_in_cache\n");
    //return cache.count(key);
    return false;
}

Record Cache::get_from_cache(std::pair<std::string, std::string> key) {
    if (!cache.count(key)) {
        Record record;
        record.data = NULL;
        return record;
    }
    else {
        return cache[key];
    }
}

void Cache::delete_from_cache(std::pair<std::string, std::string> key) {
    if (!cache.count(key)) {
        return;
    }

    free(cache[key].data);
    cache.erase(key);
}

int Cache::push_to_cache(std::pair<std::string, std::string> key, char *value, size_t size_value) {
    if (cache.count(key)) {
        free(cache[key].data);
    }

    cache[key].data = (char*) malloc(size_value);
    memcpy(cache[key].data, value, size_value);

    cache[key].size = size_value;
}

Cache::~Cache() {
    for (auto elem : cache) {
        free(elem.second.data);
    }
}