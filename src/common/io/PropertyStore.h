// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

    void set_uint8(const std::string &key, uint8_t value);

    uint8_t get_uint8(const std::string &key, uint8_t default_value);

    void set_uint128(const std::string &key, uint128_t value);

    uint128_t get_uint128(const std::string &key, uint128_t default_value);


    void set_double(const std::string &key, double value);

    double get_double(const std::string &key, double default_value);
};


#endif //MOTIVO_PROPERTYSTORE_H
