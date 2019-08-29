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

#include "OptionsParser.h"

bool OptionsParser::parse(const int argc, const char **argv)
{
    bool end_of_options = false;
    int i=1;
    while(i<argc)
    {
        std::string arg(argv[i]);
        if( end_of_options || arg.compare(0, 1, "-") ) //arg does not start with "-"
        {
            positional_args.push_back(arg);
            i++;
            continue;
        }

        if(arg == "--") //option is "--". End of option arguments
        {
            end_of_options = true;
            i++;
            continue;
        }

        unsigned int opt_idx = 0;
        bool found = false;
        if( arg.compare(0, 2, "--") ) //arg does not start with "--". Short option
        {
            if(arg.length()!=2)
                return false; //option is "-" or has more than one other character

            for(unsigned int j=0; j<options.size(); j++)
            {
                if(arg[1]==options[j]->short_name)
                {
                    found = true;
                    opt_idx = j;
                    break;
                }
            }
        }
        else //Long option
        {
            std::string name = arg.substr(2);
            for (unsigned int j = 0; j < options.size(); j++)
            {
                if(name == options[j]->name)
                {
                    found = true;
                    opt_idx = j;
                    break;
                }
            }
        }

        if(!found) //unrecognized option
            return false;

        options[opt_idx]->set_found();
        if(options[opt_idx]->requires_argument)
        {
            if(++i>=argc) //No more arguments
                return  false;

            options[opt_idx]->set_value( argv[i] );
        }

        i++;
    }

    return true;
}

OptionsParser::Option *OptionsParser::add_option(bool required, bool requires_argument, std::string name, char short_name, const std::string& default_value, const std::string& help)
{
    Option* opt = new Option(required, requires_argument, std::move(name), short_name, default_value, help);
    options.push_back(opt);
    return opt;
}

OptionsParser::~OptionsParser()
{
    for(auto &option : options)
        delete option;
}

std::string OptionsParser::help()
{
    std::string h;
    for(auto &option : options)
    {
        std::string line;
        if(option->name.length()!=0)
        {
            line += "--";
            line += option->name;
        }

        if(option->short_name!='\0')
        {
            line += (h.length() ? " | " : "");
            line += "-";
            line += option->short_name;
        }

        if(option->requires_argument)
            line += " ARG";

        if(line.length()<20)
            line += std::string(20-line.length(), ' ');

        line += "\t";
        line += option->help;
        line += "\n";

        h += line;
    }

    return h;
}

bool OptionsParser::has_required_options()
{
    for(auto &option : options)
    {
        if(option->required && !option->is_found())
            return false;
    }

    return true;
}

