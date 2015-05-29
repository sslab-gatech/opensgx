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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

#include <getopt.h>
#include <iostream>

#include "cmd_line_parser.h"
#include "utils.h"
#include "debug.h"

#define DISABLE_ABBREVIATE


CmdLineParser::Option::Option(int in_id, const std::string& in_name, char in_short_name,
                              OptionType in_type, const std::string& in_help,
                              const std::string& in_arg_name)
    : id (in_id)
    , name (in_name)
    , arg_name (in_arg_name)
    , type (in_type)
    , short_name (in_short_name)
    , help (in_help)
    , optional (true)
    , is_set (false)
    , separator (0)
{
}

CmdLineParser::CmdLineParser(std::string description, bool allow_positional_args)
    : _description (description)
    , _short_options ("+")
    , _argc (0)
    , _argv (NULL)
    , _multi_args (NULL)
    , _multi_next (NULL)
    , _multi_separator (0)
    , _positional_args (allow_positional_args)
    , _done (false)
{
    //Enables multiple instantiations. One at a time, not thread-safe.
    optind = 1;
    opterr = 1;
    optopt = 0;
    optarg = 0;
}

CmdLineParser::~CmdLineParser()
{
    Options::iterator iter = _options.begin();
    for (; iter != _options.end(); ++iter) {
        delete *iter;
    }
    delete[] _multi_args;
}

void CmdLineParser::add_private(int id, const std::string& name, char short_name,
                                OptionType type, const std::string& help,
                                const std::string& arg_name)
{
    if (_argv) {
        THROW("unexpected");
    }

    if (find(id)) {
        THROW("exist");
    }

    if (name.size() == 0) {
        THROW("invalid name");
    }

    if (find(name)) {
        THROW("name exist");
    }

    if (short_name != 0) {
        if (!isalnum(short_name) || short_name == 'W') {
            THROW("invalid short name");
        }

        if (find(short_name)) {
            THROW("short name exist");
        }
    }

    if (help.size() == 0) {
        THROW("invalid help string");
    }

    if (help.find_first_of('\t') != std::string::npos) {
        THROW("tab is not allow in help string");
    }

    _options.push_back(new Option(id, name, short_name, type, help, arg_name));
}

void CmdLineParser::add(int id, const std::string& name, const std::string& help, char short_name)
{
    if (id < OPTION_FIRST_AVAILABLE) {
        THROW("invalid id");
    }
    add_private(id, name, short_name, NO_ARGUMENT, help, "");
}

void CmdLineParser::add(int id, const std::string& name, const std::string& help,
                        const std::string& arg_name, bool required_arg, char short_name)
{
    if (id < OPTION_FIRST_AVAILABLE) {
        THROW("invalid id");
    }

    if (arg_name.size() == 0) {
        THROW("invalid arg name");
    }

    add_private(id, name, short_name, required_arg ? REQUIRED_ARGUMENT : OPTIONAL_ARGUMENT, help,
                arg_name);
}

void CmdLineParser::set_multi(int id, char separator)
{
    if (_argv) {
        THROW("unexpected");
    }

    if (!ispunct(separator)) {
        THROW("invalid separator");
    }

    Option* opt = find(id);

    if (!opt) {
        THROW("not found");
    }

    if (opt->type == NO_ARGUMENT) {
        THROW("can't set multi for option without argument");
    }

    opt->separator = separator;
}

void CmdLineParser::set_required(int id)
{
    if (_argv) {
        THROW("unexpected");
    }

    Option* opt = find(id);

    if (!opt) {
        THROW("not found");
    }

    opt->optional = false;
}

CmdLineParser::Option* CmdLineParser::find(int id)
{
    Options::iterator iter = _options.begin();
    for (; iter != _options.end(); ++iter) {
        if ((*iter)->id == id) {
            return *iter;
        }
    }
    return NULL;
}

bool CmdLineParser::is_set(int id)
{
    Option *opt = find(id);

    if (!opt) {
        THROW("not found");
    }
    return opt->is_set;
}

CmdLineParser::Option* CmdLineParser::find(const std::string& name)
{
    Options::iterator iter = _options.begin();
    for (; iter != _options.end(); ++iter) {
        if ((*iter)->name == name) {
            return *iter;
        }
    }
    return NULL;
}

CmdLineParser::Option* CmdLineParser::find(char short_name)
{
    if (short_name == 0) {
        return NULL;
    }

    Options::iterator iter = _options.begin();
    for (; iter != _options.end(); ++iter) {
        if ((*iter)->short_name == short_name) {
            return *iter;
        }
    }
    return NULL;
}

CmdLineParser::Option* CmdLineParser::find_missing_opt()
{
    Options::iterator iter = _options.begin();
    for (; iter != _options.end(); ++iter) {
        CmdLineParser::Option* opt = *iter;
        if (!opt->optional && !opt->is_set) {
            return opt;
        }
    }
    return NULL;
}

void CmdLineParser::build()
{
    Options::iterator iter = _options.begin();
    _long_options.resize(_options.size() + 1);
    for (int i = 0; iter != _options.end(); ++iter, i++) {
        CmdLineParser::Option* opt = *iter;
        struct option& long_option = _long_options[i];
        long_option.name = opt->name.c_str();
        switch (opt->type) {
        case NO_ARGUMENT:
            long_option.has_arg = no_argument;
            break;
        case OPTIONAL_ARGUMENT:
            long_option.has_arg = optional_argument;
            break;
        case REQUIRED_ARGUMENT:
            long_option.has_arg = required_argument;
            break;
        }
        long_option.flag = &long_option.val;
        long_option.val = opt->id;
        if (opt->short_name != 0) {
            _short_options += opt->short_name;
            switch (opt->type) {
            case OPTIONAL_ARGUMENT:
                _short_options += "::";
                break;
            case REQUIRED_ARGUMENT:
                _short_options += ":";
                break;
            case NO_ARGUMENT:
                break;
            }
        }
    }
    struct option& long_option = _long_options[_long_options.size() - 1];
    long_option.flag = 0;
    long_option.has_arg = 0;
    long_option.name = NULL;
    long_option.val = 0;
}

void CmdLineParser::begin(int argc, char** argv)
{
    if (_argv) {
        THROW("unexpected");
    }

    if (!argv || argc < 1) {
        THROW("invalid args");
    }

    add_private(CmdLineParser::OPTION_HELP, "help", 0, NO_ARGUMENT, "show command help", "");
    opterr = 0;
    _argv = argv;
    _argc = argc;
    build();
}

char* CmdLineParser::start_multi(char *optarg, char separator)
{
    if (!optarg) {
        return NULL;
    }
    _multi_args = new char[strlen(optarg) + 1];
    _multi_separator = separator;
    strcpy(_multi_args, optarg);
    if ((_multi_next = strchr(_multi_args, _multi_separator))) {
        *(_multi_next++) = 0;
    }
    return _multi_args;
}

char* CmdLineParser::next_multi()
{
    if (!_multi_next) {
        _multi_separator = 0;
        delete[] _multi_args;
        _multi_args = NULL;
        return NULL;
    }
    char* ret = _multi_next;
    if ((_multi_next = strchr(_multi_next, _multi_separator))) {
        *(_multi_next++) = 0;
    }

    return ret;
}

int CmdLineParser::get_option(char** val)
{
    CmdLineParser::Option* opt_obj;

    if (!_argv) {
        THROW("unexpected");
    }

    if (_multi_args) {
        THROW("in multi args mode");
    }

    if (_done) {
        THROW("is done");
    }

    int long_index;

    int opt = getopt_long(_argc, _argv, _short_options.c_str(), &_long_options[0], &long_index);

    switch (opt) {
    case 0: {
        if (!(opt_obj = find(_long_options[long_index].val))) {
            THROW("long option no found");
        }

#ifdef DISABLE_ABBREVIATE
        int name_pos =
            (opt_obj->type == REQUIRED_ARGUMENT && optarg[-1] != '=')
            ? optind - 2
            : optind - 1;
        std::string cmd_name(_argv[name_pos] + 2);
        if (cmd_name.find(opt_obj->name) != 0) {
            Platform::term_printf("%s: invalid abbreviated option '--%s'\n", _argv[0], cmd_name.c_str());
            return OPTION_ERROR;
        }
#endif

        if (opt_obj->separator) {
            *val = start_multi(optarg, opt_obj->separator);
        } else {
            *val = optarg;
        }
        opt_obj->is_set = true;
        return opt_obj->id;
    }
    case -1: {
        *val = NULL;
        if (!_positional_args && optind != _argc) {
            Platform::term_printf("%s: unexpected positional arguments\n", _argv[0]);
            return OPTION_ERROR;
        }
        if ((opt_obj = find_missing_opt())) {
            Platform::term_printf("%s: option --%s is required\n", _argv[0], opt_obj->name.c_str());
            return OPTION_ERROR;
        }
        _done = true;
        return OPTION_DONE;
    }
    case '?':
        if (optopt >= 255) {
            opt_obj = find(optopt);
            ASSERT(opt_obj);

#ifdef DISABLE_ABBREVIATE
            std::string cmd_name(_argv[optind - 1] + 2);
            if (cmd_name.find(opt_obj->name) != 0) {
                Platform::term_printf("%s: invalid option '--%s'\n", _argv[0], cmd_name.c_str());
                return OPTION_ERROR;
            }
#endif
            Platform::term_printf("%s: option --%s requires an argument\n",
                                  _argv[0], opt_obj->name.c_str());
        } else if (optopt == 0) {
            Platform::term_printf("%s: invalid option '%s'\n", _argv[0], _argv[optind - 1]);
        } else if ((opt_obj = find((char)optopt))) {
            Platform::term_printf("%s: option '-%c' requires an argument\n",
                                  _argv[0], opt_obj->short_name);
        } else {
            Platform::term_printf("%s: invalid option '-%c'\n", _argv[0], char(optopt));
        }
        return OPTION_ERROR;
    default:
        if (opt > 255 || !(opt_obj = find((char)opt))) {
            *val = NULL;
            return OPTION_ERROR;
        }
        if (opt_obj->separator) {
            *val = start_multi(optarg, opt_obj->separator);
        } else {
            *val = optarg;
        }
        opt_obj->is_set = true;
        return opt_obj->id;
    }
}

char* CmdLineParser::next_argument()
{
    if (!_argv) {
        THROW("unexpected");
    }

    if (_multi_args) {
        return next_multi();
    }

    if (!_done) {
        THROW("not done");
    }

    if (optind == _argc) {
        return NULL;
    }
    return _argv[optind++];
}

void CmdLineParser::show_help()
{
    static const int HELP_START_POS = 30;
    static const unsigned HELP_WIDTH = 80 - HELP_START_POS;
    std::ostringstream os;

    os << _argv[0] << " - " << _description.c_str() << "\n\noptions:\n\n";

    Options::iterator iter = _options.begin();
    for (; iter != _options.end(); ++iter) {
        CmdLineParser::Option* opt = *iter;

        if (opt->short_name) {
            os << "  -" << opt->short_name << ", ";
        } else {
            os << "      ";
        }

        os << "--" << opt->name;

        if (opt->type == OPTIONAL_ARGUMENT) {
            os << "[=";
        } else if (opt->type == REQUIRED_ARGUMENT) {
            os << " <";
        }

        if (opt->type == OPTIONAL_ARGUMENT || opt->type == REQUIRED_ARGUMENT) {
            if (opt->separator) {
                os << opt->arg_name << opt->separator << opt->arg_name << "...";
            } else {
                os << opt->arg_name;
            }
        }

        if (opt->type == OPTIONAL_ARGUMENT) {
            os << "]";
        } else if (opt->type == REQUIRED_ARGUMENT) {
            os << ">";
        }

        int skip = HELP_START_POS - os.str().size();
        if (skip < 2) {
            os << "\n                              ";
        } else {
            while (skip--) {
                os << " ";
            }
        }

        int line_count = 0;
        std::istringstream is(opt->help);
        std::string line;
        std::getline(is, line);
        do {
            if (line_count++) {
                os << "                              ";
            }
            if (line.size() > HELP_WIDTH) {
                size_t last_space, now = HELP_WIDTH;
                std::string sub;
                sub.append(line, 0, now);
                if ((last_space = sub.find_last_of(' ')) != std::string::npos) {
                    now = last_space;
                    sub.resize(now++);
                }
                os << sub << "\n";
                line = line.substr(now, line.size() - now);
            } else {
                os << line << "\n";
                line.clear();
            }
        } while (line.size() || std::getline(is, line));
    }

    os << "\n";
    Platform::term_printf("%s", os.str().c_str());
}
