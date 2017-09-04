/*-
 * Copyright (C) 2014-2017 Pietro Cerutti <gahr@gahr.ch>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

#include <cstdlib>
#include <ctime>
using namespace std;

#include "x11_interface.h"

class X11XCB : public X11Interface {

    public:
        X11XCB();
        virtual ~X11XCB();
        virtual bool createWindow(int width, int height);
        virtual bool setupGC(const string& bgColor, const string& fgColor, const string& fontDesc);
        virtual bool grabKeyboard();
        virtual bool redraw(const string& command, string::size_type cursorPos);
        virtual bool nextEvent(X11Event &ev);

    private:
        uint32_t parseColorName(const string& colorName);
        xcb_query_text_extents_reply_t * getTextExtent(const string& s, int len);

    private:
        xcb_connection_t  * m_connection;
        xcb_screen_t      * m_screen;
        xcb_window_t        m_win;
        xcb_key_symbols_t * m_keysyms;
        xcb_font_t          m_font;
        xcb_gcontext_t      m_fgGc;
        xcb_gcontext_t      m_bgGc;

        uint16_t m_width;
        uint16_t m_height;
};

X11Interface *
X11Interface::create()
{
    return new X11XCB();
}

X11XCB::X11XCB()
    : m_connection(nullptr)
{ }

X11XCB::~X11XCB()
{
    if (!m_connection)
        return;

    free(m_keysyms);
    xcb_close_font(m_connection, m_font);
    xcb_free_gc(m_connection, m_fgGc);
    xcb_free_gc(m_connection, m_bgGc);
    xcb_destroy_window(m_connection, m_win);
    xcb_disconnect(m_connection);
}

bool
X11XCB::createWindow(int width, int height)
{
    m_width = width;
    m_height = height;

    /* open connection to the display server */
    if ((m_connection = xcb_connect(NULL, NULL)) == nullptr) {
        return false;
    }
    m_screen = xcb_setup_roots_iterator(xcb_get_setup(m_connection)).data;

    /* allocate keysyms */
    m_keysyms = xcb_key_symbols_alloc(m_connection);
    if (m_keysyms == nullptr) {
        return false;
    }

    /* figure out the window location */
    int top { m_screen->height_in_pixels / 2 - height / 2 };
    int left { m_screen->width_in_pixels / 2 - width / 2 };

    /* create the window */
    uint32_t mask { XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK };
    uint32_t value[] { 1, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS };
    m_win = xcb_generate_id(m_connection);
    auto createCookie = xcb_create_window_checked(m_connection, XCB_COPY_FROM_PARENT, m_win, m_screen->root,
            left, top, width, height, 0, 0, m_screen->root_visual, mask, value);

    /* set wm hints */
    xcb_size_hints_t hints;
    hints.flags = XCB_ICCCM_SIZE_HINT_P_SIZE | XCB_ICCCM_SIZE_HINT_P_POSITION | XCB_ICCCM_SIZE_HINT_P_SIZE | 
                  XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | XCB_ICCCM_SIZE_HINT_P_MAX_SIZE;
    hints.x = top;
    hints.y = left;
    hints.min_width = hints.max_width = width;
    hints.min_height = hints.max_height = height;
    xcb_icccm_set_wm_normal_hints_checked(m_connection, m_win, &hints);

    /* map the window */
    auto mapCookie = xcb_map_window_checked(m_connection, m_win);

    if (xcb_request_check(m_connection, createCookie)) {
        return false;
    }

    if (xcb_request_check(m_connection, mapCookie)) {
        return false;
    }

    return true;
}

uint32_t
X11XCB::parseColorName(const string& colorName)
{
     auto lc = xcb_lookup_color(m_connection, m_screen->default_colormap, colorName.size(), colorName.c_str());
     auto lr = xcb_lookup_color_reply(m_connection, lc, NULL);
     auto ac = xcb_alloc_color(m_connection, m_screen->default_colormap, lr->exact_red, lr->exact_green, lr->exact_blue);
     auto ar = xcb_alloc_color_reply(m_connection, ac, NULL);

     uint32_t color { ar->pixel };

     free(lr);
     free(ar);

     return color;
}

xcb_query_text_extents_reply_t *
X11XCB::getTextExtent(const string& s, int len)
{
    xcb_char2b_t chars[len];
    for (int i = 0; i < len; ++i) {
        chars[i].byte1 = 0;
        chars[i].byte2 = s[i];
    }

    auto cookie = xcb_query_text_extents(m_connection, m_font, len, chars);
    auto reply = xcb_query_text_extents_reply(m_connection, cookie, NULL);

    return reply;
}

bool
X11XCB::setupGC(const string& bgColorName, const string& fgColorName, const string& fontDesc)
{
    /* open font */
    m_font = xcb_generate_id(m_connection);
    auto fontCookie = xcb_open_font_checked(m_connection, m_font, fontDesc.size(), fontDesc.c_str());
    if (xcb_request_check(m_connection, fontCookie)) {
        return false;
    }

    /* resolve colors */
    auto bgColor = parseColorName(bgColorName);
    auto fgColor = parseColorName(fgColorName);

    /* create gc */
    uint32_t gcMask { XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_LINE_WIDTH | XCB_GC_LINE_STYLE | XCB_GC_CAP_STYLE | XCB_GC_JOIN_STYLE | XCB_GC_FONT };
    uint32_t gcValues[] { fgColor, bgColor, 1, XCB_LINE_STYLE_SOLID, XCB_CAP_STYLE_BUTT, XCB_JOIN_STYLE_BEVEL, m_font };
    m_fgGc = xcb_generate_id(m_connection);
    auto fgGcCookie = xcb_create_gc_checked(m_connection, m_fgGc, m_win, gcMask, gcValues);

    /* create rectangle gc */
    uint32_t rectgcMask { XCB_GC_FOREGROUND | XCB_GC_BACKGROUND };
    uint32_t rectgcValues[] { bgColor, bgColor };
    m_bgGc = xcb_generate_id(m_connection);
    auto bgGcCookie = xcb_create_gc_checked(m_connection, m_bgGc, m_win, rectgcMask, rectgcValues);

    if (xcb_request_check(m_connection, fgGcCookie)) {
        return false;
    }
    if (xcb_request_check(m_connection, bgGcCookie)) {
        return false;
    }

    return true;
}

bool
X11XCB::grabKeyboard()
{
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 5000000;
    unsigned long maxwait { 3000000000UL }; /* 3 seconds */
    unsigned int i;

    /* this loop is required since pwm grabs the keyboard during the event loop */
    for (i = 0; i < (maxwait / req.tv_nsec); i++) {
        nanosleep(&req, NULL);
        auto cookie = xcb_grab_keyboard(m_connection, 1, m_win, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        auto reply = xcb_grab_keyboard_reply(m_connection, cookie, NULL);
        if (reply && reply->status == XCB_GRAB_STATUS_SUCCESS) {
            free(reply);
            xcb_set_input_focus(m_connection, XCB_INPUT_FOCUS_PARENT, m_win, XCB_CURRENT_TIME);
            return true;
        }
    }

    return false;
}

bool
X11XCB::redraw(const string& command, string::size_type cursorPos)
{
    /* draw the background rectangle */
    xcb_rectangle_t extRect { 0, 0, m_width, m_height };
    auto bgCookie = xcb_poly_fill_rectangle_checked(m_connection, m_win, m_bgGc, 1, &extRect);

    /* draw the foreground rectangle */
    uint16_t w = m_width - 1;
    uint16_t h = m_height - 1;
    xcb_rectangle_t intRect { 0, 0, w, h };
    auto fgCookie = xcb_poly_rectangle_checked(m_connection, m_win, m_fgGc, 1, &intRect);

    /* get text size */
    auto wholeExt = getTextExtent(command, command.size());
    auto partialExt = getTextExtent(command, cursorPos);

    /* draw the text */
    auto txtCookie = xcb_image_text_8_checked(m_connection, command.size(), m_win, m_fgGc, 2,
        m_height/2 + wholeExt->font_ascent/2, command.c_str());

    /* draw the cursor */
    int16_t cursorLeft = partialExt->overall_width + 2;
    xcb_rectangle_t curRect = { cursorLeft, 6, 1, 16 };
    auto curCookie = xcb_poly_fill_rectangle_checked(m_connection, m_win, m_fgGc, 1, &curRect);

    free(wholeExt);
    free(partialExt);

    if (xcb_request_check(m_connection, bgCookie)) {
        return false;
    }
    if (xcb_request_check(m_connection, fgCookie)) {
        return false;
    }
    if (xcb_request_check(m_connection, txtCookie)) {
        return false;
    }
    if (xcb_request_check(m_connection, curCookie)) {
        return false;
    }

    xcb_flush(m_connection);

    return true;
}

bool
X11XCB::nextEvent(X11Event& event)
{
    xcb_generic_event_t * e;
    xcb_key_press_event_t * kev;

    event.type = X11Event::EventType::Evt_Other;

    e = xcb_wait_for_event(m_connection);
    if (!e) {
        return false;
    }

    switch (e->response_type & ~0x80) {
        case XCB_EXPOSE:
            event.type = X11Event::EventType::Evt_Expose;
            break;
        case XCB_KEY_PRESS:
            kev = reinterpret_cast<xcb_key_press_event_t *>(e);
            event.type = X11Event::EventType::Evt_KeyPress;
            event.key = xcb_key_symbols_get_keysym(m_keysyms, kev->detail, 0);
            event.state = kev->state;
            break;

        default:
            break;
    }

    free(e);

    return true;
}
