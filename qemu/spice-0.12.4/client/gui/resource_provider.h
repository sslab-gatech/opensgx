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
#ifndef _H_RESOURCE_PROVIDER
#define _H_RESOURCE_PROVIDER

/* CEGUI 0.6 bug, CEGUITexture.h doesn't include this, we need to */
#include <cstddef>

#include "CEGUIDefaultResourceProvider.h"

class CEGUIResourceProvider: public CEGUI::ResourceProvider {
public:
    virtual void loadRawDataContainer(const CEGUI::String &filename,
                                      CEGUI::RawDataContainer &output,
                                      const CEGUI::String &resourceGroup);

    virtual void unloadRawDataContainer(CEGUI::RawDataContainer& data);
};

enum {
    STR_INVALID,
    STR_MESG_MISSING_HOST_NAME,
    STR_MESG_INVALID_PORT,
    STR_MESG_INVALID_SPORT,
    STR_MESG_MISSING_PORT,
    STR_MESG_CONNECTING,
    STR_BUTTON_OK,
    STR_BUTTON_CANCEL,
    STR_BUTTON_CONNECT,
    STR_BUTTON_QUIT,
    STR_BUTTON_CLOSE,
    STR_BUTTON_DISCONNECT,
    STR_BUTTON_OPTIONS,
    STR_BUTTON_BACK,
    STR_LABEL_HOST,
    STR_LABEL_PORT,
    STR_LABEL_SPORT,
    STR_LABEL_PASSWORD,
};

//todo: move to x11/res.cpp and make x11/res.cpp cross-platform
const char* res_get_string(int id);

#endif
