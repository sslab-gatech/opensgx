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
#include <map>
#include "platform_utils.h"
#include "utils.h"
#include "threads.h"

void string_vprintf(std::string& str, const char* format, va_list ap)
{
    int buf_size = 256;
    for (;;) {
        AutoArray<char> buf(new char[buf_size]);
        int r = vsnprintf_s(buf.get(), buf_size, buf_size - 1, format, ap);
        if (r != -1) {
            str = buf.get();
            return;
        }
        buf_size *= 2;
    }
}

HDC create_compatible_dc()
{
    HDC dc = CreateCompatibleDC(NULL);
    if (!dc) {
        THROW("create compatible DC failed");
    }
    return dc;
}

HBITMAP get_bitmap_res(int id)
{
    HBITMAP bitmap = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(id));
    if (!bitmap) {
        THROW("get bitmap #%d failed", id);
    }
    return bitmap;
}

HBITMAP get_alpha_bitmap_res(int id)
{
    AutoGDIObject bitmap(LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(id), IMAGE_BITMAP, 0, 0,
                                   LR_DEFAULTCOLOR | LR_CREATEDIBSECTION | LR_SHARED));
    if (!bitmap.valid()) {
        THROW("get alpha bitmap #%d failed", id);
    }

    BITMAP src_info;
    GetObject(bitmap.get(), sizeof(src_info), &src_info);
    if (src_info.bmBitsPixel != 32 || src_info.bmPlanes != 1) {
        THROW("invalid format #%d ", id);
    }

    LONG src_size = src_info.bmHeight * src_info.bmWidthBytes;
    AutoArray<uint8_t> src_pixels(new uint8_t[src_size]);
    LONG ncopy = GetBitmapBits((HBITMAP)bitmap.get(), src_size, src_pixels.get());
    if (ncopy != src_size) {
        THROW("get vitmap bits failed, %u", GetLastError());
    }

    AutoDC auto_dc(create_compatible_dc());
    BITMAPINFO dest_info;
    uint8_t* dest;
    dest_info.bmiHeader.biSize = sizeof(dest_info.bmiHeader);
    dest_info.bmiHeader.biWidth = src_info.bmWidth;
    dest_info.bmiHeader.biHeight = -src_info.bmHeight;
    dest_info.bmiHeader.biPlanes = 1;
    dest_info.bmiHeader.biBitCount = 32;
    dest_info.bmiHeader.biCompression = BI_RGB;
    dest_info.bmiHeader.biSizeImage = 0;
    dest_info.bmiHeader.biXPelsPerMeter = dest_info.bmiHeader.biYPelsPerMeter = 0;
    dest_info.bmiHeader.biClrUsed = 0;
    dest_info.bmiHeader.biClrImportant = 0;

    HBITMAP ret = CreateDIBSection(auto_dc.get(), &dest_info, 0, (VOID**)&dest, NULL, 0);
    if (!ret) {
        THROW("create bitmap failed, %u", GetLastError());
    }

    uint8_t* src_line = src_pixels.get();
    for (int i = 0; i < src_info.bmHeight; i++, src_line += src_info.bmWidthBytes) {
        uint8_t* src = src_line;
        for (int j = 0; j < src_info.bmWidth; j++) {
            dest[3] = src[3];
            double alpha = (double)dest[3] / 0xff;
            dest[2] = (uint8_t)(alpha * src[2]);
            dest[1] = (uint8_t)(alpha * src[1]);
            dest[0] = (uint8_t)(alpha * src[0]);
            src += 4;
            dest += 4;
        }
    }
    return ret;
}

static std::map<int, const char*> errors_map;
static Mutex errors_map_mutex;

const char* sys_err_to_str(int error)
{
    Lock lock(errors_map_mutex);
    if (errors_map.find(error) == errors_map.end()) {
        LPSTR msg;
        if (!FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error,
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            (LPSTR)&msg, 0, NULL)) {
            const int BUF_SIZE = 20;
            msg = new char[BUF_SIZE];
            _snprintf(msg, BUF_SIZE, "errno %d", error);
        } else {
            char* new_line;
            if ((new_line = strrchr(msg, '\r'))) {
                *new_line = 0;
            }
        }
        errors_map[error] = msg;
    }
    return errors_map[error];
}

int inet_aton(const char* ip, struct in_addr* in_addr)
{
    unsigned long addr = inet_addr(ip);

    if (addr == INADDR_NONE) {
        return 0;
    }
    in_addr->S_un.S_addr = addr;
    return 1;
}
