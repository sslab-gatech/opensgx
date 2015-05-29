/*
   Copyright (C) 2009 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _H_CMD_LINE_PARSER
#define _H_CMD_LINE_PARSER

class CmdLineParser {
public:

    enum {
        OPTION_ERROR = -1,
        OPTION_DONE = 0,
        OPTION_HELP = 256,
        OPTION_FIRST_AVAILABLE,
    };

    CmdLineParser(std::string description, bool allow_positional_args);
    virtual ~CmdLineParser();

    void add(int id, const std::string& name, const std::string& help,
             char short_name = 0);

    void add(int id, const std::string& name, const std::string& help,
             const std::string& arg_name, bool required_arg, char short_name = 0);
    void set_multi(int id, char separator);
    void set_required(int id);

    void begin(int argc, char** argv);
    int get_option(char** val);
    char* next_argument();
    bool is_set(int id);

    void show_help();

private:
    class Option;

    enum OptionType {
        NO_ARGUMENT,
        OPTIONAL_ARGUMENT,
        REQUIRED_ARGUMENT,
    };

    void add_private(int id, const std::string& name, char short_name, OptionType type,
                     const std::string& help, const std::string& arg_name);
    Option* find(char short_name);
    Option* find(int id);
    Option* find(const std::string& name);
    Option* find_missing_opt();

    void build();

    char* start_multi(char *optarg, char separator);
    char* next_multi();

private:

    class Option {
    public:
        Option(int in_id, const std::string& in_name, char in_short_name, OptionType in_type,
               const std::string& in_help, const std::string& arg_name);

    public:
        int id;
        std::string name;
        std::string arg_name;
        OptionType type;
        char short_name;
        std::string help;
        bool optional;
        bool is_set;
        char separator;
    };

    std::string _description;
    std::vector<struct option> _long_options;
    std::string _short_options;

    typedef std::list<Option*> Options;
    Options _options;
    int _argc;
    char** _argv;
    char* _multi_args;
    char* _multi_next;
    char _multi_separator;
    bool _positional_args;
    bool _done;
};

#endif
