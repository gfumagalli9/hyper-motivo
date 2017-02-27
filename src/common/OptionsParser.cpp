//
// Created by steven on 2/27/17.
//

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

        if(!arg.compare("--")) //option is "--". End of option arguments
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
                if (!name.compare(options[j]->name))
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
    Option* opt = new Option(required, requires_argument, name, short_name, default_value, help);
    options.push_back(opt);
    return opt;
}

OptionsParser::~OptionsParser()
{
    for(unsigned int j=0; j<options.size(); j++)
        delete options[j];
}

std::string OptionsParser::help()
{
    std::string h="";
    for(unsigned int j=0; j<options.size(); j++)
    {
        std::string line="";
        if(options[j]->name.length()!=0)
        {
            line += "--";
            line += options[j]->name;
        }

        if(options[j]->short_name!='\0')
        {
            line += (h.length() ? " | " : "");
            line += "-";
            line += options[j]->short_name;
        }

        if(options[j]->requires_argument)
            line += " ARG";

        if(line.length()<20)
            line += std::string(20-line.length(), ' ');

        line += "\t";
        line += options[j]->help;
        line += "\n";

        h += line;
    }

    return h;
}

bool OptionsParser::has_required_options()
{
    for(unsigned int j=0; j<options.size(); j++)
    {
        if(options[j]->required && !options[j]->is_found())
            return false;
    }

    return true;
}

