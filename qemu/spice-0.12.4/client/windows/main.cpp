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
#include <fstream>
#include <windows.h>
extern "C" {
#include "pthread.h"
}

#include "application.h"
#include "debug.h"
#include "utils.h"

HINSTANCE instance = NULL;

static void init_winsock()
{
    WSADATA wsaData;
    int res;

    if ((res = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        THROW("WSAStartup failed %d", res);
    }
}

#ifdef __MINGW32__
// XXX: for mingw32 we can do both actually, but it seems easier
// to just use the autoconf provided PACKAGE_VERSION.
static void init_version_string()
{
}
#else
const char* PACKAGE_VERSION = "???";
static char _version_string[40];

static void init_version_string()
{
    DWORD handle;
    DWORD verrsion_inf_size = GetFileVersionInfoSizeA(__argv[0], &handle);
    if (verrsion_inf_size == 0) {
        return;
    }
    AutoArray<uint8_t> info_buf (new uint8_t[verrsion_inf_size]);
    if (!GetFileVersionInfoA(__argv[0], handle, verrsion_inf_size, info_buf.get())) {
         return;
    }
    UINT size;
    VS_FIXEDFILEINFO *file_info;
    if (!VerQueryValueA(info_buf.get(), "\\", (VOID**)&file_info, &size) ||
            size < sizeof(VS_FIXEDFILEINFO)) {
        return;
    }
    sprintf(_version_string, "%d.%d.%d.%d",
        (int)(file_info->dwFileVersionMS >> 16),
        (int)(file_info->dwFileVersionMS & 0x0ffff),
        (int)(file_info->dwFileVersionLS >> 16),
        (int)(file_info->dwFileVersionLS & 0x0ffff));
    PACKAGE_VERSION = _version_string;
}
#endif

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
    int exit_val;

    instance = hInstance;

    try {
        init_version_string();
        pthread_win32_process_attach_np();
        init_winsock();
        exit_val = Application::main(__argc, __argv, PACKAGE_VERSION);
        LOG_INFO("Spice client terminated (exitcode = %d)", exit_val);
    } catch (Exception& e) {
        LOG_ERROR("unhandle exception: %s", e.what());
        exit_val = e.get_error_code();
    } catch (std::exception& e) {
        LOG_ERROR("unhandle exception: %s", e.what());
        exit_val = SPICEC_ERROR_CODE_ERROR;
    } catch (...) {
        LOG_ERROR("unhandled exception");
        exit_val = SPICEC_ERROR_CODE_ERROR;
    }

    spice_log_cleanup();
    pthread_win32_process_detach_np();

    return exit_val;
}
