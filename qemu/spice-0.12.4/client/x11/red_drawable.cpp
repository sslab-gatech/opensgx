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
#include "red_drawable.h"
#include "pixels_source_p.h"
#include "debug.h"
#include "x_platform.h"
#include "utils.h"

#ifdef USE_OPENGL
#include "common/gl_utils.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

static inline void copy_to_gldrawable_from_gltexture(const RedDrawable_p* dest,
                                                     const SpiceRect& area,
                                                     const SpicePoint& offset,
                                                     const PixelsSource_p* source,
                                                     int src_x, int src_y)
{
    float text_x1, text_x2;
    float text_y1, text_y2;
    int vertex_x1, vertex_x2;
    int vertex_y1, vertex_y2;
    GLXPbuffer pbuffer;
    GLXContext context;
    RenderType rendertype;
    Window win;

    text_x1 = (float)src_x / source->gl.width_powed;
    text_x2 = text_x1 + (float)(area.right - area.left) / source->gl.width_powed;

    text_y1 = ((float)source->gl.height - (area.bottom - area.top) - src_y) /
              source->gl.height_powed;
    text_y2 = text_y1 + (float)(area.bottom - area.top) / source->gl.height_powed;

    vertex_x1 = area.left + offset.x;
    vertex_y1 = dest->source.x_drawable.height - (area.top + offset.y) - (area.bottom - area.top);
    vertex_x2 = vertex_x1 + (area.right - area.left);
    vertex_y2 = vertex_y1 + (area.bottom - area.top);

    glEnable(GL_TEXTURE_2D);

    rendertype = source->gl.rendertype;
    if (rendertype == RENDER_TYPE_FBO) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    } else {
        win = source->gl.win;
        context = source->gl.context;
        glXMakeCurrent(XPlatform::get_display(), win, context);
    }

    glBindTexture(GL_TEXTURE_2D, source->gl.tex);

    glBegin(GL_QUADS);
    glTexCoord2f(text_x1, text_y1);
    glVertex2i(vertex_x1, vertex_y1);
    glTexCoord2f(text_x1, text_y2);
    glVertex2i(vertex_x1, vertex_y2);
    glTexCoord2f(text_x2, text_y2);
    glVertex2i(vertex_x2, vertex_y2);
    glTexCoord2f(text_x2, text_y1);
    glVertex2i(vertex_x2, vertex_y1);
    glEnd();

    if (rendertype == RENDER_TYPE_FBO) {
        GLuint fbo;

        fbo = source->gl.fbo;
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    } else {
        pbuffer = source->gl.pbuff;
        glXMakeCurrent(XPlatform::get_display(), pbuffer, context);
    }
}

static inline void copy_to_gldrawable_from_pixmap(const RedDrawable_p* dest,
                                                  const SpiceRect& area,
                                                  const SpicePoint& offset,
                                                  const PixelsSource_p* source,
                                                  int src_x, int src_y)
{
    uint8_t *addr;
    GLXContext context = NULL;
    GLXPbuffer pbuffer;
    RenderType rendertype;
    Window win;

    rendertype = dest->source.x_drawable.rendertype;
    if (rendertype == RENDER_TYPE_FBO) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    } else {
        context = dest->source.x_drawable.context;
        win = dest->source.x_drawable.drawable;
        glXMakeCurrent(XPlatform::get_display(), win, context);
        glDisable(GL_TEXTURE_2D);
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, pixman_image_get_stride(source->pixmap.pixman_image) / 4);

    glPixelZoom(1, -1);
    addr = (uint8_t *)pixman_image_get_data(source->pixmap.pixman_image);
    addr += (src_x * 4 + src_y * pixman_image_get_stride(source->pixmap.pixman_image));
    glWindowPos2i(area.left + offset.x, dest->source.x_drawable.height -
                  (area.top + offset.y));  //+ (area.bottom - area.top)));
    glDrawPixels(area.right - area.left, area.bottom - area.top,
                 GL_BGRA, GL_UNSIGNED_BYTE, addr);

    if (rendertype == RENDER_TYPE_FBO) {
        GLuint fbo;

        fbo = dest->source.x_drawable.fbo;
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    } else {
        pbuffer = dest->source.x_drawable.pbuff;
        glXMakeCurrent(XPlatform::get_display(), pbuffer, context);
    }
}
#endif // USE_OPENGL

static inline void copy_to_drawable_from_drawable(const RedDrawable_p* dest,
                                                  const SpiceRect& area,
                                                  const SpicePoint& offset,
                                                  const PixelsSource_p* source,
                                                  int src_x, int src_y)
{
    XGCValues gc_vals;
    gc_vals.function = GXcopy;

    XChangeGC(XPlatform::get_display(), dest->source.x_drawable.gc, GCFunction, &gc_vals);
    XCopyArea(XPlatform::get_display(), source->x_drawable.drawable,
              dest->source.x_drawable.drawable, dest->source.x_drawable.gc,
              src_x, src_y,
              area.right - area.left, area.bottom - area.top,
              area.left + offset.x, area.top + offset.y);
}

static XImage *create_temp_image(int screen, int width, int height,
                                 pixman_image_t **pixman_image_out,
                                 XShmSegmentInfo **shminfo_out)
{
    XImage *image;
    XShmSegmentInfo *shminfo;
    RedDrawable::Format format;
    pixman_image_t *pixman_image;
    XVisualInfo *vinfo;

    image = NULL;
    shminfo = NULL;

    vinfo = XPlatform::get_vinfo()[screen];
    format = XPlatform::get_screen_format(screen);

    image = XPlatform::create_x_image(format, width, height, vinfo->depth,
					  vinfo->visual, &shminfo);

    pixman_image = pixman_image_create_bits(RedDrawable::format_to_pixman(format),
                                            width, height,
                                            (uint32_t *)image->data, image->bytes_per_line);
    if (pixman_image == NULL) {
        THROW("surf create failed");
    }
    *pixman_image_out = pixman_image;
    *shminfo_out = shminfo;
    return image;
}

static void free_temp_image(XImage *image, XShmSegmentInfo *shminfo, pixman_image_t *pixman_image)
{
    XPlatform::free_x_image(image, shminfo);
    pixman_image_unref(pixman_image);
}


static inline void copy_to_drawable_from_pixmap(const RedDrawable_p* dest,
                                                const SpiceRect& area,
                                                const SpicePoint& offset,
                                                const PixelsSource_p* source,
                                                int src_x, int src_y)
{
    pixman_image_t *src_surface = source->pixmap.pixman_image;
    XGCValues gc_vals;
    gc_vals.function = GXcopy;
    RedDrawable::Format screen_format;
    XImage *image;
    XShmSegmentInfo *shminfo;
    pixman_image_t *pixman_image;
    int screen;

    screen = dest->source.x_drawable.screen;
    screen_format = XPlatform::get_screen_format(screen);

    XChangeGC(XPlatform::get_display(), dest->source.x_drawable.gc, GCFunction, &gc_vals);

    if (source->pixmap.x_image != NULL &&
        RedDrawable::format_copy_compatible(source->pixmap.format, screen_format)) {
        if (source->pixmap.shminfo) {
            XShmPutImage(XPlatform::get_display(), dest->source.x_drawable.drawable,
                         dest->source.x_drawable.gc, source->pixmap.x_image,
                         src_x, src_y, area.left + offset.x, area.top + offset.y,
                         area.right - area.left, area.bottom - area.top, false);
        } else {
            XPutImage(XPlatform::get_display(), dest->source.x_drawable.drawable,
                      dest->source.x_drawable.gc, source->pixmap.x_image, src_x,
                      src_y, area.left + offset.x, area.top + offset.y,
                      area.right - area.left, area.bottom - area.top);
        }
    } else {
        image = create_temp_image(screen,
                                  area.right - area.left, area.bottom - area.top,
                                  &pixman_image, &shminfo);

        pixman_image_composite32(PIXMAN_OP_SRC,
                                 src_surface, NULL, pixman_image,
                                 src_x + offset.x,
                                 src_y + offset.y,
                                 0, 0,
                                 0, 0,
                                 area.right - area.left,
                                 area.bottom - area.top);

        if (shminfo) {
            XShmPutImage(XPlatform::get_display(), dest->source.x_drawable.drawable,
                         dest->source.x_drawable.gc, image,
                         0, 0, area.left + offset.x, area.top + offset.y,
                         area.right - area.left, area.bottom - area.top, false);
        } else {
            XPutImage(XPlatform::get_display(), dest->source.x_drawable.drawable,
                      dest->source.x_drawable.gc, image,
                      0, 0, area.left + offset.x, area.top + offset.y,
                      area.right - area.left, area.bottom - area.top);
        }

        free_temp_image(image, shminfo, pixman_image);
    }
    XFlush(XPlatform::get_display());
}

static inline void copy_to_x_drawable(const RedDrawable_p* dest,
                                      const SpiceRect& area,
                                      const SpicePoint& offset,
                                      const PixelsSource_p* source,
                                      int src_x, int src_y)
{
    switch (source->type) {
    case PIXELS_SOURCE_TYPE_X_DRAWABLE:
        copy_to_drawable_from_drawable(dest, area, offset, source, src_x, src_y);
        break;
    case PIXELS_SOURCE_TYPE_PIXMAP:
        copy_to_drawable_from_pixmap(dest, area, offset, source, src_x, src_y);
        break;
    default:
        THROW("invalid source type %d", source->type);
    }
}

#ifdef USE_OPENGL
static inline void copy_to_gl_drawable(const RedDrawable_p* dest,
                                       const SpiceRect& area,
                                       const SpicePoint& offset,
                                       const PixelsSource_p* source,
                                       int src_x, int src_y)
{
    switch (source->type) {
    case PIXELS_SOURCE_TYPE_GL_TEXTURE:
        copy_to_gldrawable_from_gltexture(dest, area, offset, source, src_x, src_y);
        break;
    case PIXELS_SOURCE_TYPE_PIXMAP:
        copy_to_gldrawable_from_pixmap(dest, area, offset, source, src_x, src_y);
        break;
    default:
        THROW("invalid source type %d", source->type);
    }
}
#endif // USE_OPENGL

static inline void copy_to_pixmap_from_drawable(const RedDrawable_p* dest,
                                                const SpiceRect& area,
                                                const SpicePoint& offset,
                                                const PixelsSource_p* source,
                                                int src_x, int src_y)
{
    LOG_WARN("not implemented");
}

static inline void copy_to_pixmap_from_pixmap(const RedDrawable_p* dest,
                                              const SpiceRect& area,
                                              const SpicePoint& offset,
                                              const PixelsSource_p* source,
                                              int src_x, int src_y)
{
    pixman_image_t *dest_surface =  dest->source.pixmap.pixman_image;
    pixman_image_t *src_surface = source->pixmap.pixman_image;

    pixman_image_composite32(PIXMAN_OP_SRC,
                             src_surface, NULL, dest_surface,
                             src_x + offset.x,
                             src_y + offset.y,
                             0, 0,
                             area.left + offset.x,
                             area.top + offset.y,
                             area.right - area.left,
                             area.bottom - area.top);
}

#ifdef USE_OPENGL
static inline void copy_to_pixmap_from_gltexture(const RedDrawable_p* dest,
                                                 const SpiceRect& area,
                                                 const SpicePoint& offset,
                                                 const PixelsSource_p* source,
                                                 int src_x, int src_y)
{
    int y, height;
    GLXContext context = NULL;
    GLXPbuffer pbuffer;
    Window win;
    RenderType rendertype;

    y = source->gl.height - src_y;
    height = area.bottom - area.top;

    rendertype = source->gl.rendertype;
    if (rendertype == RENDER_TYPE_FBO) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, source->gl.fbo);
        glBindTexture(GL_TEXTURE_2D, 0);
    } else {
        context = source->gl.context;
        pbuffer = source->gl.pbuff;
        glXMakeCurrent(XPlatform::get_display(), pbuffer, context);
        glDisable(GL_TEXTURE_2D);
    }
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glPixelStorei(GL_PACK_ROW_LENGTH,
                  pixman_image_get_stride(dest->source.pixmap.pixman_image) / 4);

    while (height > 0) {
        glReadPixels(src_x, y - height, area.right - area.left, 1,
                     GL_BGRA, GL_UNSIGNED_BYTE,
                     (uint8_t *)pixman_image_get_data(dest->source.pixmap.pixman_image) +
                     (area.left + offset.x) * 4 +
                     (area.top + offset.y + height - 1) *
                     pixman_image_get_stride(dest->source.pixmap.pixman_image));
        height--;
    }
    if (rendertype != RENDER_TYPE_FBO) {
        win = source->gl.win;
        glXMakeCurrent(XPlatform::get_display(), win, context);
    }
}
#endif // USE_OPENGL

static inline void copy_to_pixmap(const RedDrawable_p* dest,
                                  const SpiceRect& area,
                                  const SpicePoint& offset,
                                  const PixelsSource_p* source,
                                  int src_x, int src_y)
{
    switch (source->type) {
#ifdef USE_OPENGL
    case PIXELS_SOURCE_TYPE_GL_TEXTURE:
        copy_to_pixmap_from_gltexture(dest, area, offset, source, src_x, src_y);
        break;
#endif // USE_OPENGL
    case PIXELS_SOURCE_TYPE_X_DRAWABLE:
        copy_to_pixmap_from_drawable(dest, area, offset, source, src_x, src_y);
        break;
    case PIXELS_SOURCE_TYPE_PIXMAP:
        copy_to_pixmap_from_pixmap(dest, area, offset, source, src_x, src_y);
        break;
    default:
        THROW("invalid source type %d", source->type);
    }
}

void RedDrawable::copy_pixels(const PixelsSource& src, int src_x, int src_y, const SpiceRect& area)
{
    PixelsSource_p* source = (PixelsSource_p*)src.get_opaque();
    RedDrawable_p* dest = (RedDrawable_p*)get_opaque();
    switch (dest->source.type) {
#ifdef USE_OPENGL
    case PIXELS_SOURCE_TYPE_GL_DRAWABLE:
        copy_to_gl_drawable(dest, area, _origin, source, src_x + src._origin.x,
                            src_y + src._origin.y);
        break;
#endif // USE_OPENGL
    case PIXELS_SOURCE_TYPE_X_DRAWABLE:
        copy_to_x_drawable(dest, area, _origin, source, src_x + src._origin.x,
                           src_y + src._origin.y);
        break;
    case PIXELS_SOURCE_TYPE_PIXMAP:
        copy_to_pixmap(dest, area, _origin, source, src_x + src._origin.x, src_y + src._origin.y);
        break;
    default:
        THROW("invalid dest type %d", dest->source.type);
    }
}

static inline void blend_to_drawable(const RedDrawable_p* dest,
                                     const SpiceRect& area,
                                     const SpicePoint& offset,
                                     const PixelsSource_p* source,
                                     int src_x, int src_y)
{
    LOG_WARN("not implemented");
}

static inline void blend_to_pixmap_from_drawable(const RedDrawable_p* dest,
                                                 const SpiceRect& area,
                                                 const SpicePoint& offset,
                                                 const PixelsSource_p* source,
                                                 int src_x, int src_y)
{
    LOG_WARN("not implemented");
}

static inline void blend_to_pixmap_from_pixmap(const RedDrawable_p* dest,
                                               const SpiceRect& area,
                                               const SpicePoint& offset,
                                               const PixelsSource_p* source,
                                               int src_x, int src_y)
{
    pixman_image_t *dest_surface =  dest->source.pixmap.pixman_image;
    pixman_image_t *src_surface = source->pixmap.pixman_image;

    pixman_image_composite32 (PIXMAN_OP_ATOP,
                              src_surface, NULL, dest_surface,
                              src_x + offset.x,
                              src_y + offset.y,
                              0, 0,
                              area.left + offset.x,
                              area.top + offset.y,
                              area.right - area.left,
                              area.bottom - area.top);
}

static inline void blend_to_pixmap(const RedDrawable_p* dest,
                                   const SpiceRect& area,
                                   const SpicePoint& offset,
                                   const PixelsSource_p* source,
                                   int src_x, int src_y)
{
    switch (source->type) {
    case PIXELS_SOURCE_TYPE_X_DRAWABLE:
        blend_to_pixmap_from_drawable(dest, area, offset, source, src_x, src_y);
        break;
    case PIXELS_SOURCE_TYPE_PIXMAP:
        blend_to_pixmap_from_pixmap(dest, area, offset, source, src_x, src_y);
        break;
    default:
        THROW("invalid source type %d", source->type);
    }
}

void RedDrawable::blend_pixels(const PixelsSource& src, int src_x, int src_y, const SpiceRect& area)
{
    PixelsSource_p* source = (PixelsSource_p*)src.get_opaque();
    RedDrawable_p* dest = (RedDrawable_p*)get_opaque();
    switch (dest->source.type) {
    case PIXELS_SOURCE_TYPE_X_DRAWABLE:
        blend_to_drawable(dest, area, _origin, source, src_x + src._origin.x,
                          src_y + src._origin.y);
        break;
    case PIXELS_SOURCE_TYPE_PIXMAP:
        blend_to_pixmap(dest, area, _origin, source, src_x + src._origin.x, src_y + src._origin.y);
        break;
    default:
        THROW("invalid dest type %d", dest->source.type);
    }
}

static inline void combine_to_drawable(const RedDrawable_p* dest,
                                       const SpiceRect& area,
                                       const SpicePoint& offset,
                                       const PixelsSource_p* source,
                                       int src_x, int src_y,
                                       RedDrawable::CombineOP op)
{
    LOG_WARN("not implemented");
}

static inline void combine_to_pixmap_from_drawable(const RedDrawable_p* dest,
                                                   const SpiceRect& area,
                                                   const SpicePoint& offset,
                                                   const PixelsSource_p* source,
                                                   int src_x, int src_y,
                                                   RedDrawable::CombineOP op)
{
    LOG_WARN("not implemented");
}

static inline void combine_to_pixmap_from_pixmap(const RedDrawable_p* dest,
                                                 const SpiceRect& area,
                                                 const SpicePoint& offset,
                                                 const PixelsSource_p* source,
                                                 int src_x, int src_y,
                                                 RedDrawable::CombineOP op)
{
    pixman_image_t *dest_surface =  dest->source.pixmap.pixman_image;
    pixman_image_t *src_surface = source->pixmap.pixman_image;

    SpiceROP rop;
    switch (op) {
    case RedDrawable::OP_COPY:
        rop = SPICE_ROP_COPY;
        break;
    case RedDrawable::OP_AND:
        rop = SPICE_ROP_AND;
        break;
    case RedDrawable::OP_XOR:
        rop = SPICE_ROP_XOR;
        break;
    default:
        THROW("invalid op %d", op);
    }


    if (pixman_image_get_depth (src_surface) == 1) {
        pixman_color_t white = { 0xffff, 0xffff, 0xffff, 0xffff };
        pixman_image_t *solid;
        pixman_image_t *temp;

        /* Create a temporary rgb32 image that is black where mask is 0
           and white where mask is 1 */
        temp = pixman_image_create_bits(pixman_image_get_depth(dest_surface) == 24 ?
                                        PIXMAN_x8r8g8b8 : PIXMAN_a8r8g8b8,
                                        area.right - area.left,
                                        area.bottom - area.top, NULL, 0);
        solid = pixman_image_create_solid_fill(&white);
        pixman_image_composite32(PIXMAN_OP_SRC,
                                 solid, src_surface, temp,
                                 0, 0,
                                 src_x + offset.x,
                                 src_y + offset.y,
                                 0, 0,
                                 area.right - area.left,
                                 area.bottom - area.top);
        pixman_image_unref(solid);

        /* ROP the temp image on the destination */
        spice_pixman_blit_rop(dest_surface,
                              temp,
                              0,
                              0,
                              area.left + offset.x,
                              area.top + offset.y,
                              area.right - area.left,
                              area.bottom - area.top,
                              rop);
        pixman_image_unref(temp);

    } else {
        spice_pixman_blit_rop(dest_surface,
                              src_surface,
                              src_x + offset.x,
                              src_y + offset.y,
                              area.left + offset.x,
                              area.top + offset.y,
                              area.right - area.left,
                              area.bottom - area.top,
                              rop);
    }
}

static inline void combine_to_pixmap(const RedDrawable_p* dest,
                                     const SpiceRect& area,
                                     const SpicePoint& offset,
                                     const PixelsSource_p* source,
                                     int src_x, int src_y,
                                     RedDrawable::CombineOP op)
{
    switch (source->type) {
    case PIXELS_SOURCE_TYPE_X_DRAWABLE:
        combine_to_pixmap_from_drawable(dest, area, offset, source, src_x, src_y, op);
        break;
    case PIXELS_SOURCE_TYPE_PIXMAP:
        combine_to_pixmap_from_pixmap(dest, area, offset, source, src_x, src_y, op);
        break;
    default:
        THROW("invalid source type %d", source->type);
    }
}

void RedDrawable::combine_pixels(const PixelsSource& src, int src_x, int src_y, const SpiceRect& area,
                                 CombineOP op)
{
    PixelsSource_p* source = (PixelsSource_p*)src.get_opaque();
    RedDrawable_p* dest = (RedDrawable_p*)get_opaque();
    switch (dest->source.type) {
    case PIXELS_SOURCE_TYPE_X_DRAWABLE:
        combine_to_drawable(dest, area, _origin, source, src_x + src._origin.x,
                            src_y + src._origin.y, op);
        break;
    case PIXELS_SOURCE_TYPE_PIXMAP:
        combine_to_pixmap(dest, area, _origin, source, src_x + src._origin.x,
                          src_y + src._origin.y, op);
        break;
    default:
        THROW("invalid dest type %d", dest->source.type);
    }
}

void RedDrawable::erase_rect(const SpiceRect& area, rgb32_t color)
{
    LOG_WARN("not implemented");
}

static inline void fill_drawable(RedDrawable_p* dest, const SpiceRect& area, rgb32_t color,
                                 const SpicePoint& offset)
{
    Drawable drawable = dest->source.x_drawable.drawable;
    GC gc = dest->source.x_drawable.gc;

    XLockDisplay(XPlatform::get_display());
    Colormap color_map = DefaultColormap(XPlatform::get_display(),
                                         DefaultScreen(XPlatform::get_display()));
    XColor x_color;
    x_color.red = (uint16_t)rgb32_get_red(color) << 8;
    x_color.green = (uint16_t)rgb32_get_green(color) << 8;
    x_color.blue = (uint16_t)rgb32_get_blue(color) << 8;
    x_color.flags = DoRed | DoGreen | DoBlue;
    //todo: optimize color map
    if (!XAllocColor(XPlatform::get_display(), color_map, &x_color)) {
        LOG_WARN("color map failed");
    }
    XUnlockDisplay(XPlatform::get_display());

    XGCValues gc_vals;
    gc_vals.foreground = x_color.pixel;
    gc_vals.function = GXcopy;
    gc_vals.fill_style = FillSolid;
    XChangeGC(XPlatform::get_display(), gc, GCFunction | GCForeground | GCFillStyle, &gc_vals);
    XFillRectangle(XPlatform::get_display(), drawable,
                   gc, area.left + offset.x, area.top + offset.y,
                   area.right - area.left, area.bottom - area.top);
}

#ifdef USE_OPENGL
static inline void fill_gl_drawable(RedDrawable_p* dest, const SpiceRect& area, rgb32_t color,
                                    const SpicePoint& offset)
{
    int vertex_x1, vertex_x2;
    int vertex_y1, vertex_y2;
    GLXContext context;

    context = glXGetCurrentContext();
    if (!context) {
        return;
    }

    vertex_x1 = area.left + offset.x;
    vertex_y1 = dest->source.x_drawable.height - (area.top + offset.y) - (area.bottom - area.top);

    vertex_x2 = vertex_x1 + (area.right - area.left);
    vertex_y2 = vertex_y1 + (area.bottom - area.top);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    glColor3f(rgb32_get_red(color), rgb32_get_green(color),
              rgb32_get_blue(color));

    glBegin(GL_QUADS);
    glVertex2i(vertex_x1, vertex_y1);
    glVertex2i(vertex_x1, vertex_y2);
    glVertex2i(vertex_x2, vertex_y2);
    glVertex2i(vertex_x2, vertex_y1);
    glEnd();
    glFlush();

    glColor3f(1, 1, 1);
}
#endif // USE_OPENGL

static inline void fill_pixmap(RedDrawable_p* dest, const SpiceRect& area, rgb32_t color,
                               const SpicePoint& offset)
{
    pixman_image_t *dest_surface =  dest->source.pixmap.pixman_image;

    spice_pixman_fill_rect(dest_surface,
                           area.left + offset.x, area.top + offset.y,
                           area.right - area.left,
                           area.bottom - area.top,
                           color);
}

void RedDrawable::fill_rect(const SpiceRect& area, rgb32_t color)
{
    RedDrawable_p* dest = (RedDrawable_p*)get_opaque();
    switch (dest->source.type) {
#ifdef USE_OPENGL
    case PIXELS_SOURCE_TYPE_GL_DRAWABLE:
        fill_gl_drawable(dest, area, color, _origin);
        break;
#endif // USE_OPENGL
    case PIXELS_SOURCE_TYPE_X_DRAWABLE:
        fill_drawable(dest, area, color, _origin);
        break;
    case PIXELS_SOURCE_TYPE_PIXMAP:
        fill_pixmap(dest, area, color, _origin);
        break;
    default:
        THROW("invalid dest type %d", dest->source.type);
    }
}

static inline void frame_drawable(RedDrawable_p* dest, const SpiceRect& area, rgb32_t color,
                                  const SpicePoint& offset)
{
    Drawable drawable = dest->source.x_drawable.drawable;
    GC gc = dest->source.x_drawable.gc;

    XLockDisplay(XPlatform::get_display());
    Colormap color_map = DefaultColormap(XPlatform::get_display(),
                                         DefaultScreen(XPlatform::get_display()));
    XColor x_color;
    x_color.red = (uint16_t)rgb32_get_red(color) << 8;
    x_color.green = (uint16_t)rgb32_get_green(color) << 8;
    x_color.blue = (uint16_t)rgb32_get_blue(color) << 8;
    x_color.flags = DoRed | DoGreen | DoBlue;
    //todo: optimize color map
    if (!XAllocColor(XPlatform::get_display(), color_map, &x_color)) {
        LOG_WARN("color map failed");
    }
    XUnlockDisplay(XPlatform::get_display());

    XGCValues gc_vals;
    gc_vals.foreground = x_color.pixel;
    gc_vals.function = GXcopy;
    gc_vals.fill_style = FillSolid;
    XChangeGC(XPlatform::get_display(), gc, GCFunction | GCForeground | GCFillStyle, &gc_vals);
    XFillRectangle(XPlatform::get_display(), drawable,
                   gc, area.left + offset.x, area.top + offset.y,
                   area.right - area.left, area.bottom - area.top);
}

static inline void frame_pixmap(RedDrawable_p* dest, const SpiceRect& area, rgb32_t color,
                                const SpicePoint& offset)
{
    pixman_image_t *dest_surface =  dest->source.pixmap.pixman_image;

    spice_pixman_fill_rect(dest_surface,
                           area.left + offset.x, area.top + offset.y,
                           area.right - area.left,
                           1,
                           color);
    spice_pixman_fill_rect(dest_surface,
                           area.left + offset.x, area.bottom + offset.y,
                           area.right - area.left,
                           1,
                           color);
    spice_pixman_fill_rect(dest_surface,
                           area.left + offset.x, area.top + offset.y,
                           1,
                           area.bottom - area.top,
                           color);
    spice_pixman_fill_rect(dest_surface,
                           area.right + offset.x, area.top + offset.y,
                           1,
                           area.bottom - area.top,
                           color);
}

void RedDrawable::frame_rect(const SpiceRect& area, rgb32_t color)
{
    RedDrawable_p* dest = (RedDrawable_p*)get_opaque();
    switch (dest->source.type) {
    case PIXELS_SOURCE_TYPE_X_DRAWABLE:
        frame_drawable(dest, area, color, _origin);
        break;
    case PIXELS_SOURCE_TYPE_PIXMAP:
        frame_pixmap(dest, area, color, _origin);
        break;
    default:
        THROW("invalid dest type %d", dest->source.type);
    }
}
