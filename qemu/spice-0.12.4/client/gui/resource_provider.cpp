/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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

#include "resource_provider.h"
#include "debug.h"
#include "utils.h"

#include "CEGUIExceptions.h"

#include "taharez_look.scheme.c"
#include "taharez_look.imageset.c"
#include "taharez_look.tga.c"
#include "commonwealth-10.font.c"
#include "commonv2c.ttf.c"
#include "taharez_look.looknfeel.c"
#include "dejavu_sans-10.font.c"
#include "dejavu_sans.ttf.c"

//(echo "const unsigned char <struct name>[] =
//{"; od -txC -v <in file name> | sed -e "s/^[0-9]*//" -e s"/ \([0-9a-f][0-9a-f]\)/0x\1,/g" -e"\$d"
//| sed -e"\$s/,$/};/") > <out file name>.c

void CEGUIResourceProvider::loadRawDataContainer(const CEGUI::String &filename,
                                              CEGUI::RawDataContainer &output,
                                              const CEGUI::String &resourceGroup)
{
    DBG(0, "%s", filename.c_str());
    if (strcmp(filename.c_str(), "TaharezLook.scheme") == 0) {
        DBG(0, "size %d", sizeof(taharez_look_schem));
         output.setData((CEGUI::uint8*)taharez_look_schem);
         output.setSize(sizeof(taharez_look_schem));
         return;
    }

    if (strcmp(filename.c_str(), "TaharezLook.imageset") == 0) {
        DBG(0, "size %d", sizeof(taharez_look_imageset));
        output.setData((CEGUI::uint8*)taharez_look_imageset);
        output.setSize(sizeof(taharez_look_imageset));
        return;
    }

    if (strcmp(filename.c_str(), "TaharezLook.tga") == 0) {
        DBG(0, "size %d", sizeof(taharez_look_tga));
        output.setData((CEGUI::uint8*)taharez_look_tga);
        output.setSize(sizeof(taharez_look_tga));
        return;
    }

    if (strcmp(filename.c_str(), "Commonwealth-10.font") == 0) {
        DBG(0, "size %d", sizeof(commonwealth_10_font));
        output.setData((CEGUI::uint8*)commonwealth_10_font);
        output.setSize(sizeof(commonwealth_10_font));
        return;
    }

    if (strcmp(filename.c_str(), "Commonv2c.ttf") == 0) {
        DBG(0, "size %d", sizeof(commonv2c_ttf));
        output.setData((CEGUI::uint8*)commonv2c_ttf);
        output.setSize(sizeof(commonv2c_ttf));
        return;
    }

    if (strcmp(filename.c_str(), "TaharezLook.looknfeel") == 0) {
        DBG(0, "size %d", sizeof(taharez_look_looknfeel));
        output.setData((CEGUI::uint8*)taharez_look_looknfeel);
        output.setSize(sizeof(taharez_look_looknfeel));
        return;
    }

    if (strcmp(filename.c_str(), "DejaVuSans-10.font") == 0) {
        DBG(0, "size %d", sizeof(dejavu_sans_10_font));
        output.setData((CEGUI::uint8*)dejavu_sans_10_font);
        output.setSize(sizeof(dejavu_sans_10_font));
        return;
    }

    if (strcmp(filename.c_str(), "DejaVuSans.ttf") == 0) {
        DBG(0, "size %d", sizeof(dejavu_sans_ttf));
        output.setData((CEGUI::uint8*)dejavu_sans_ttf);
        output.setSize(sizeof(dejavu_sans_ttf));
        return;
    }

    throw CEGUI::GenericException("failed");
}

void CEGUIResourceProvider::unloadRawDataContainer(CEGUI::RawDataContainer& data)
{
    data.setData(NULL);
    data.setSize(0);
}

struct ResString{
    int id;
    const char* str;
} res_strings[] = {
    {STR_MESG_MISSING_HOST_NAME, "Missing host name"},
    {STR_MESG_INVALID_PORT, "Invalid port"},
    {STR_MESG_INVALID_SPORT, "Invalid sport"},
    {STR_MESG_MISSING_PORT, "Missing port"},
    {STR_MESG_CONNECTING, "Connecting to"},
    {STR_BUTTON_OK, "OK"},
    {STR_BUTTON_CANCEL, "Cancel"},
    {STR_BUTTON_CONNECT, "Connect"},
    {STR_BUTTON_QUIT, "Quit"},
    {STR_BUTTON_CLOSE, "Close"},
    {STR_BUTTON_DISCONNECT, "Disconnect"},
    {STR_BUTTON_OPTIONS, "Options"},
    {STR_BUTTON_BACK, "Back"},
    {STR_LABEL_HOST, "Host"},
    {STR_LABEL_PORT, "Port"},
    {STR_LABEL_SPORT, "Secure port"},
    {STR_LABEL_PASSWORD, "Password"},
    {0, NULL},
};

const char* res_get_string(int id)
{
    ResString *string;

    for (string = res_strings; string->str; string++) {
        if (string->id == id) {
            return string->str;
        }
    }

    return NULL;
}
