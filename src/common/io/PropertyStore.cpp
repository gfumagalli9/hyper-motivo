//
// Created by steven on 1/19/19.
//

#include "PropertyStore.h"

#include <fstream>
#include <sstream>
#include <string>

bool PropertyStore::is_valid(const std::string &key)
{
    for(char c : key)
        if(c<'!' || c>'~')
            return false;

    return key.length()>=1;
}

PropertyStore::PropertyStore(const std::string &filename)
{
    std::ifstream file(filename);
    if(file.fail())
        throw std::runtime_error("Error reading file");

    std::string line;
    while(std::getline(file, line))
    {
        size_t pos = line.find(' ');
        if(pos==0 || pos == std::string::npos)
            continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos+1);

        map[key]=value;
    }

    if(!file.eof())
        throw std::runtime_error("Error reading file");

    file.close();
}

void PropertyStore::save(const std::string &filename)
{
    std::ofstream file(filename);
    if(file.fail())
        throw std::runtime_error("Error reading file");

    for(const auto &[key, value] : map)
    {
        file << key << " " << value << "\n";

        if(file.fail())
            throw std::runtime_error("Error reading file");
    }

    file.close();
}

void PropertyStore::set_string(const std::string &key, const std::string &value)
{
    if(!is_valid((key)) || ! is_valid(value))
        throw std::runtime_error("Invalid character in string argument");

    map[key]=value;
}

const std::string &PropertyStore::get_string(const std::string &key, const std::string &default_value)
{
    auto it = map.find(key);
    if(it == map.end())
        return default_value;

    return it->second;
}

void PropertyStore::set_bool(const std::string &key, const bool value)
{
    map[key] = value?"y":"n";
}

bool PropertyStore::get_bool(const std::string &key, bool default_value)
{
    auto it = map.find(key);
    if(it == map.end() || it->second.length()!=1)
        return default_value;

    if(it->second[0]=='y')
        return true;

    if(it->second[0]=='n')
        return false;

    return default_value;
}

void PropertyStore::set_uint128(const std::string &key, const uint128_t value)
{
    map[key] = uint128_to_string(value);
}

uint128_t PropertyStore::get_uint128(const std::string &key, const uint128_t default_value)
{
    auto it = map.find(key);
    if(it == map.end())
        return default_value;

    return string_to_uint128(it->second);
}

void PropertyStore::set_double(const std::string &key, const double value)
{
    std::ostringstream ss;
    ss.precision(std::numeric_limits<double>::max_digits10);
    ss << value;

    map[key] = ss.str();
}

double PropertyStore::get_double(const std::string &key, double default_value)
{
    auto it = map.find(key);
    if(it == map.end())
        return default_value;


    return std::stod(it->second);
}
