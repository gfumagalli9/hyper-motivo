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
