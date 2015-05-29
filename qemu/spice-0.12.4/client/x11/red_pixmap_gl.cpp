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

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <X11/Xlib.h>
#include "common/gl_utils.h"

#include "common.h"
#include "red_pixmap_gl.h"
#include "debug.h"
#include "utils.h"
#include "pixels_source_p.h"
#include "x_platform.h"
#include "red_window_p.h"


RedPixmapGL::RedPixmapGL(int width, int height, RedDrawable::Format format,
                         bool top_bottom, RedWindow *win,
                         RenderType rendertype)
    : RedPixmap(width, height, format, top_bottom)
{
    GLuint fbo;
    GLuint tex;
    GLuint stencil_tex = 0;
    Win xwin;
    //GLint max_texture_size;

    ASSERT(format == RedDrawable::ARGB32 || format == RedDrawable::RGB32 || format == RedDrawable::A1);
    ASSERT(sizeof(RedDrawable_p) <= PIXELES_SOURCE_OPAQUE_SIZE);

    ((PixelsSource_p*)get_opaque())->type = PIXELS_SOURCE_TYPE_GL_TEXTURE;

    _glcont = win->create_context_gl();
    if (!_glcont) {
        THROW("glXCreateContext failed");
    }

    win->set_gl_context(_glcont);

    xwin = ((RedWindow_p*)win)->get_window();

    /*glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    if (width > max_texture_size || height > max_texture_size) {
        throw Exception(fmt("%s: unsuported max %|| width %|| height %||")
                             % __FUNCTION__
                             % max_texture_size
                             % width
                             % height);
    }*/

    if (rendertype == RENDER_TYPE_FBO) {
        glXMakeCurrent(XPlatform::get_display(), xwin, _glcont);
        if (!gluCheckExtension((GLubyte *)"GL_EXT_framebuffer_object",
                               glGetString(GL_EXTENSIONS))) {
            glXMakeCurrent(XPlatform::get_display(), 0, 0);
            glXDestroyContext(XPlatform::get_display(), _glcont);
            THROW("no GL_EXT_framebuffer_object extension");
        }
        glEnable(GL_TEXTURE_2D);
        glGenFramebuffersEXT(1, &fbo);
        glGenTextures(1, &tex);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);

        glBindTexture(GL_TEXTURE_2D, tex);

        glTexImage2D(GL_TEXTURE_2D, 0, 4, gl_get_to_power_two(width),
                     gl_get_to_power_two(height), 0, GL_BGRA, GL_UNSIGNED_BYTE,
                     NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                  GL_TEXTURE_2D, tex, 0);


        glGenTextures(1, &stencil_tex);
        glBindTexture(GL_TEXTURE_2D, stencil_tex);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL_EXT,
                     gl_get_to_power_two(width), gl_get_to_power_two(height), 0,
                     GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, NULL);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                                  GL_DEPTH_ATTACHMENT_EXT,
                                  GL_TEXTURE_2D, stencil_tex, 0);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                                  GL_STENCIL_ATTACHMENT_EXT,
                                  GL_TEXTURE_2D, stencil_tex, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        ((PixelsSource_p*)get_opaque())->gl.fbo = fbo;
        win->set_render_fbo(fbo);
    } else {
        GLXPbuffer pbuff;

        pbuff = win->create_pbuff(gl_get_to_power_two(width),
                                  gl_get_to_power_two(height));
        if (!pbuff) {
            glXDestroyContext(XPlatform::get_display(), _glcont);
            THROW("pbuff creation failed");
        }
        glXMakeCurrent(XPlatform::get_display(), pbuff, _glcont);
        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexImage2D(GL_TEXTURE_2D, 0, 4, gl_get_to_power_two(width),
                     gl_get_to_power_two(height), 0, GL_BGRA, GL_UNSIGNED_BYTE,
                     NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);
        ((PixelsSource_p*)get_opaque())->gl.pbuff = pbuff;
        win->set_render_pbuff(pbuff);
    }

    ((PixelsSource_p*)get_opaque())->gl.stencil_tex = stencil_tex;
    ((PixelsSource_p*)get_opaque())->gl.tex = tex;
    ((PixelsSource_p*)get_opaque())->gl.width = width;
    ((PixelsSource_p*)get_opaque())->gl.height = height;
    ((PixelsSource_p*)get_opaque())->gl.width_powed = gl_get_to_power_two(width);
    ((PixelsSource_p*)get_opaque())->gl.height_powed = gl_get_to_power_two(height);
    ((PixelsSource_p*)get_opaque())->gl.win = xwin;
    ((PixelsSource_p*)get_opaque())->gl.rendertype = rendertype;
    ((PixelsSource_p*)get_opaque())->gl.context = _glcont;

    _textures_lost = false;

    GLC_ERROR_TEST_FINISH;
}

void RedPixmapGL::textures_lost()
{
    _textures_lost = true;
}

void RedPixmapGL::touch_context()
{
    Win win;
    GLXPbuffer pbuff;
    RenderType rendertype;

    rendertype = ((PixelsSource_p*)get_opaque())->gl.rendertype;
    if (rendertype == RENDER_TYPE_FBO) {
        win = ((PixelsSource_p*)get_opaque())->gl.win;
        if (_glcont) {
            glXMakeCurrent(XPlatform::get_display(), win, _glcont);
        }
    } else {
        pbuff = ((PixelsSource_p*)get_opaque())->gl.pbuff;
        glXMakeCurrent(XPlatform::get_display(), pbuff, _glcont);
    }
    GLC_ERROR_TEST_FLUSH;
}

void RedPixmapGL::update_texture(const SpiceRect *bbox)
{
    RenderType rendertype;
    GLuint tex;
    int height;

    rendertype = ((PixelsSource_p*)get_opaque())->gl.rendertype;

    if (rendertype == RENDER_TYPE_PBUFF) {
        int tex_x, tex_y;
        int vertex_x1, vertex_x2;
        int vertex_y1, vertex_y2;
        int is_enabled;
        GLint prev_tex;

        height = ((PixelsSource_p*)get_opaque())->gl.height;

        tex = ((PixelsSource_p*)get_opaque())->gl.tex;

        tex_x = bbox->left;
        tex_y = height - bbox->bottom;
        vertex_x1 = bbox->left;
        vertex_y1 = height - bbox->bottom;
        vertex_x2 = vertex_x1 + (bbox->right - bbox->left);
        vertex_y2 = vertex_y1 + (bbox->bottom - bbox->top);

        is_enabled = glIsEnabled(GL_TEXTURE_2D);
        if (!is_enabled) {
            glEnable(GL_TEXTURE_2D);
        } else {
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
        }
        glBindTexture(GL_TEXTURE_2D, tex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, tex_x, tex_y, vertex_x1,
                            vertex_y1, vertex_x2 - vertex_x1,
                            vertex_y2 - vertex_y1);
        if (!is_enabled) {
            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_TEXTURE_2D);
        } else {
            glBindTexture(GL_TEXTURE_2D, prev_tex);
        }
    }
    GLC_ERROR_TEST_FLUSH;
}

void RedPixmapGL::pre_copy()
{
    glFlush();

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT |
                 GL_TRANSFORM_BIT);

    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, ((PixelsSource_p*)get_opaque())->gl.width, 0,
               ((PixelsSource_p*)get_opaque())->gl.height);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glViewport(0, 0, ((PixelsSource_p*)get_opaque())->gl.width,
               ((PixelsSource_p*)get_opaque())->gl.height);


    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_COLOR_LOGIC_OP);

    glColor3f(1, 1, 1);
}

void RedPixmapGL::past_copy()
{
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    glPopAttrib();
    glFlush();
}

RedPixmapGL::~RedPixmapGL()
{
    GLXPbuffer pbuff;
    GLuint fbo;
    RenderType rendertype;
    GLuint tex;
    GLuint stencil_tex;

    /*
     * GL textures might be destroyed by res change.
     */
    if (!_textures_lost) {
        tex = ((PixelsSource_p*)get_opaque())->gl.tex;
        stencil_tex = ((PixelsSource_p*)get_opaque())->gl.stencil_tex;
        if (tex) {
            glDeleteTextures(1, &tex);
        }
        if (stencil_tex) {
            glDeleteTextures(1, &stencil_tex);
        }
        if (_glcont) {
            glXDestroyContext(XPlatform::get_display(), _glcont);
        }
    }

    rendertype = ((PixelsSource_p*)get_opaque())->gl.rendertype;
    if (rendertype == RENDER_TYPE_FBO) {
        fbo = ((PixelsSource_p*)get_opaque())->gl.fbo;
        if (fbo) {
            glDeleteFramebuffersEXT(1, &fbo);
        }
    } else {
        pbuff = ((PixelsSource_p*)get_opaque())->gl.pbuff;
        glXDestroyPbuffer(XPlatform::get_display(), pbuff);
    }

    /*
     * Both tex and stenctil_tex are textures and therefore they are destroyed
     * when the context is gone, so we dont free them here as they might have
     * already been destroyed.
     */
    GLC_ERROR_TEST_FINISH;
}
