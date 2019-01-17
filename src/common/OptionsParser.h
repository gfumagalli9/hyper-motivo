#include <utility>

#include <utility>

#include <utility>

//
// Created by steven on 2/27/17.
//

#ifndef MOTIVO_OPTIONSPARSER_H
#define MOTIVO_OPTIONSPARSER_H

#include <string>
#include <vector>

class OptionsParser
{
public:
    class Option
    {
    public:
        const bool required;
        const bool requires_argument;
        const std::string name;
        const char short_name;
        const std::string default_value;
        const std::string help;

    private:
        std::string value;
        bool found = false;


    public:
        bool is_found() { return  found; }
        std::string get_value() { return found?value:default_value; }

        void set_found() {found=true;}
        void set_value(const std::string &value) { this->value = value; }

        Option(bool required, bool requires_argument, std::string name, char short_name, std::string default_value, std::string help)
            : required(required), requires_argument(requires_argument), name(std::move(name)), short_name(short_name),
            default_value(std::move(default_value)), help(std::move(help))
            {};
    };

private:
    std::vector<std::string> positional_args;
    std::vector<Option*> options;

public:
    Option* add_option(bool requred, bool requires_argument, std::string name, char short_name, const std::string& default_value, const std::string& help);
    bool parse(int argc, const char** argv);
    bool has_required_options();
    std::string help();

    const std::vector<std::string>& positional_arguments() { return positional_args; }

    ~OptionsParser();
};


#endif //MOTIVO_OPTIONSPARSER_H
