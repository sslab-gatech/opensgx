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
#include "application.h"

static void cleanup()
{
    spice_log_cleanup();
}

const char * version_str = VERSION;

int main(int argc, char** argv)
{
    int exit_val;

    atexit(cleanup);
    try {
        exit_val = Application::main(argc, argv, version_str);
        LOG_INFO("Spice client terminated (exitcode = %d)", exit_val);
    } catch (Exception& e) {
        LOG_ERROR("unhandled exception: %s", e.what());
        exit_val = e.get_error_code();
    } catch (std::exception& e) {
        LOG_ERROR("unhandled exception: %s", e.what());
        exit_val = SPICEC_ERROR_CODE_ERROR;
    } catch (...) {
        LOG_ERROR("unhandled exception");
        exit_val = SPICEC_ERROR_CODE_ERROR;
    }

    return exit_val;
}
