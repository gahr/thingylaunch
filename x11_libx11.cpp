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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cstdlib>
#include <ctime>
#include <stdexcept>
using namespace std;

#include "x11_interface.h"
#include "util.h"

class X11LibX11 : public X11Interface {

    public:
        X11LibX11();
        virtual ~X11LibX11();
        virtual bool createWindow(int width, int height);
        virtual bool setupGC(const string& bgColor, const string& fgColor, const string& fontDesc);
        virtual bool grabKeyboard();
        virtual bool redraw(const string& command, string::size_type cursorPos);
        virtual bool nextEvent(X11Event &ev);

    private:
        unsigned long parseColorName(const string& colorName);

    private:
        string        m_displayName;
        Display     * m_display;
        GC            m_gc;
        GC            m_rectgc;
        Window        m_win;
        XFontStruct * m_fontInfo;
        int           m_screenNum;

        uint16_t m_width;
        uint16_t m_height;
};

X11Interface *
X11Interface::create()
{
    return new X11LibX11();
}

X11LibX11::X11LibX11()
{ }

X11LibX11::~X11LibX11()
{
    XFreeFont(m_display, m_fontInfo);
    XFreeGC(m_display, m_gc);
    XFreeGC(m_display, m_rectgc);
    XUngrabKeyboard(m_display, CurrentTime);
    XDestroyWindow(m_display, m_win);
    XCloseDisplay(m_display);
}

bool
X11LibX11::createWindow(int width, int height)
{
    m_width = width;
    m_height = height;

    try {
        m_displayName = Util::getEnv("DISPLAY");
    } catch (runtime_error& e) {
        return false;
    }

    m_display = XOpenDisplay(m_displayName.c_str());
    if (m_display == NULL) {
        return false;
    }

    m_screenNum = DefaultScreen(m_display);

    /* figure out the window location */
    int top { DisplayHeight(m_display, m_screenNum) / 2 - height / 2 };
    int left { DisplayWidth(m_display, m_screenNum) / 2 - width / 2 };

    /* create the window */
    XSetWindowAttributes attrib;
    attrib.override_redirect = True;
    m_win = XCreateWindow(m_display, RootWindow(m_display, m_screenNum), left, top, width, height, 0,
                CopyFromParent, InputOutput, CopyFromParent, CWOverrideRedirect, &attrib);

    /* set up the window hints etc */
    XSizeHints * win_size_hints = XAllocSizeHints();
    if (!win_size_hints) {
        return false;
    }
    win_size_hints->flags = PMaxSize | PMinSize;
    win_size_hints->min_width = win_size_hints->max_width = width;
    win_size_hints->min_height = win_size_hints->max_height = height;
    XSetWMNormalHints(m_display, m_win, win_size_hints);
    XFree(win_size_hints);

    /* map the window */
    XMapWindow(m_display, m_win);

    return true;
}

unsigned long
X11LibX11::parseColorName(const string& colorName)
{
    XColor tmp;
    XParseColor(m_display, DefaultColormap(m_display, m_screenNum), colorName.c_str(), &tmp);
    XAllocColor(m_display, DefaultColormap(m_display, m_screenNum), &tmp);
    return tmp.pixel;
}

bool
X11LibX11::setupGC(const string& bgColorName, const string& fgColorName, const string& fontDesc)
{
    int valuemask { 0 };
    int line_width { 1 };
    int line_style { LineSolid };
    int cap_style { CapButt };
    int join_style { JoinBevel };
    XGCValues values;

    auto bgColor = parseColorName(bgColorName);
    auto fgColor = parseColorName(fgColorName);

    /* GC for text */
    m_gc = XCreateGC(m_display, m_win, valuemask, &values);
    XSetForeground(m_display, m_gc, fgColor);
    XSetBackground(m_display, m_gc, bgColor);
    XSetLineAttributes(m_display, m_gc, line_width, line_style, cap_style, join_style);
    if ((m_fontInfo = XLoadQueryFont(m_display, fontDesc.c_str())) == nullptr) {
        return false;
    }
    XSetFont(m_display, m_gc, m_fontInfo->fid);

    /* GC for rectangle */
    m_rectgc = XCreateGC(m_display, m_win, valuemask, &values);
    XSetForeground(m_display, m_rectgc, bgColor);
    XSetBackground(m_display, m_rectgc, bgColor);

    return true;
}

bool
X11LibX11::grabKeyboard()
{
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 5000000;
    unsigned long maxwait { 3000000000UL }; /* 3 seconds */
    unsigned int i;

    /* this loop is required since pwm grabs the keyboard during the event loop */
    for (i = 0; i < (maxwait / req.tv_nsec); i++) {
        nanosleep(&req, NULL);
        if (XGrabKeyboard(m_display, m_win, False, GrabModeAsync, GrabModeAsync, CurrentTime) == 0) {
            return true;
        }
    }

    return false;
}

bool
X11LibX11::redraw(const string& command, string::size_type cursorPos)
{
    int font_height { m_fontInfo->ascent + m_fontInfo->descent };
    int cursorLeft { XTextWidth(m_fontInfo, command.c_str(), cursorPos) };

    XFillRectangle(m_display, m_win, m_rectgc, 0, 0, m_width, m_height);
    XDrawRectangle(m_display, m_win, m_gc, 0, 0, m_width-1, m_height-1);
    XDrawString(m_display, m_win, m_gc, 2, font_height + 2, command.c_str(), command.size());
    XDrawLine(m_display, m_win, m_gc, 2 + cursorLeft, font_height + 4, 2 + cursorLeft + 10, font_height + 4);
    XFlush(m_display);

    return true;
}

bool
X11LibX11::nextEvent(X11Event& event)
{
    XEvent e;
    XKeyEvent *kev;
    KeySym key_symbol;
    char charPressed;

    event.type = X11Event::EventType::Evt_Other;

    XSelectInput(m_display, m_win, ExposureMask | KeyPressMask);

    XNextEvent(m_display, &e);
    switch(e.type) {
        case Expose:
            event.type = X11Event::EventType::Evt_Expose;
            break;
        case KeyPress:
            kev = &e.xkey;
            XLookupString(kev, &charPressed, 1, &key_symbol, NULL);
            event.type = X11Event::EventType::Evt_KeyPress;
            event.key = key_symbol;
            event.state = kev->state;
            break;
        default:
            break;
    }

    return true;
}
