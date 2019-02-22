//
// Created by steven on 1/19/19.
//

#ifndef MOTIVO_PROPERTYSTORE_H
#define MOTIVO_PROPERTYSTORE_H

#include "../platform/platform.h"
#include "../util.h"
#include <unordered_map>

class PropertyStore
{
private:
    std::unordered_map<std::string, std::string> map;

    static bool is_valid(const std::string &key);

public:
    PropertyStore() = default;

    explicit PropertyStore(const std::string& filename);

    bool contains(const std::string &key) { return map.find(key) != map.end(); }

    void erase(const std::string &key) { map.erase(key); }

    void clear() { map.clear(); }


    void save(const std::string& filename);


    void set_string(const std::string &key, const std::string &value);

    const std::string& get_string(const std::string &key, const std::string &default_value);


    void set_bool(const std::string &key, bool value);

    bool get_bool(const std::string &key, bool default_value);


    void set_uint128(const std::string &key, uint128_t value);

    uint128_t get_uint128(const std::string &key, uint128_t default_value);
};


#endif //MOTIVO_PROPERTYSTORE_H
