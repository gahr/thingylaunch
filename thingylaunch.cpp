/*-
  Copyright (C) 2003      Matt Johnston <matt@ucc.asn.au>
  Copyright (C) 2009-2014 Pietro Cerutti <gahr@gahr.ch>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.
  */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysymdef.h>
#include <libgen.h>
#include <time.h>
#include <unistd.h>

#include <cstdlib> // getenv
#include <iostream>

#include "completion.h"
#include "history.h"
#include "bookmark.h"

#define WINWIDTH 640
#define WINHEIGHT 25

class Thingylaunch {

    public:
        Thingylaunch();
        ~Thingylaunch();

        void run(int argc, char **argv);

    private:
        void openDisplay();
        void createWindow();
        void setupDefaults();
        void parseOptions(int argc, char **argv);
        void setupGC();
        void eventLoop();
        void grabHack();
        void redraw();
        bool keypress(XKeyEvent * keyevent);
        void execcmd();
        void die(std::string msg);

        unsigned long parseColorName(std::string s);

    private:
        /* X11 */
        Display     * m_display;
        GC            m_gc;
        GC            m_rectgc;
        Window        m_win;
        XFontStruct * m_fontInfo;
        int           m_screenNum;
        unsigned long m_bgColor;
        unsigned long m_fgColor;

        /* Completion, history, and bookmarks */
        Completion m_comp;
        History    m_hist;
        Bookmark   m_book;

        /* The command */
        std::string           m_command;
        std::string::iterator m_cursorPos;
};

Thingylaunch::Thingylaunch()
{
    m_cursorPos = m_command.end();
}

Thingylaunch::~Thingylaunch()
{
    // nothing to do...
}

void
Thingylaunch::run(int argc, char **argv)
{
    openDisplay();
    createWindow();

    setupDefaults();
    parseOptions(argc, argv);

    setupGC();

    grabHack();
    eventLoop();
}

int
main(int argc, char **argv)
{
    Thingylaunch t;
    t.run(argc, argv);

    return 0;
}

void
Thingylaunch::openDisplay()
{
    const char * display_name = getenv("DISPLAY");
    if (display_name == NULL) {
        die("DISPLAY not set");
    }

    m_display = XOpenDisplay(display_name);
    if (m_display == NULL) {
        die("Couldn't connect to DISPLAY");
    }

    m_screenNum = DefaultScreen(m_display);
}

void
Thingylaunch::createWindow()
{

    /* figure out the window location */
    int top = DisplayHeight(m_display, m_screenNum) / 2 - WINHEIGHT / 2;
    int left = DisplayWidth(m_display, m_screenNum) / 2 - WINWIDTH / 2;

    /* create the window */
    XSetWindowAttributes attrib;
    attrib.override_redirect = True;
    m_win = XCreateWindow(m_display, RootWindow(m_display, m_screenNum), left, top, WINWIDTH, WINHEIGHT, 0,
                CopyFromParent, InputOutput, CopyFromParent, CWOverrideRedirect, &attrib);

    /* set up the window hints etc */
    XSizeHints * win_size_hints = XAllocSizeHints();
    if (!win_size_hints) {
        die("out of memory allocating hints");
    }
    win_size_hints->flags = PMaxSize | PMinSize;
    win_size_hints->min_width = win_size_hints->max_width = WINWIDTH;
    win_size_hints->min_height = win_size_hints->max_height = WINHEIGHT;
    XSetWMNormalHints(m_display, m_win, win_size_hints);
    XFree(win_size_hints);

    /* map the window */
    XMapWindow(m_display, m_win);
}

void
Thingylaunch::setupDefaults()
{
    m_bgColor = BlackPixel(m_display, m_screenNum);
    m_fgColor = WhitePixel(m_display, m_screenNum);
}

void
Thingylaunch::parseOptions(int argc, char **argv)
{
    std::vector<std::string> args;
    std::copy(argv+1, argv+argc, std::back_inserter(args));

    for (auto i = args.cbegin(); i != args.cend(); ++i) {
        const auto& s = *i;

        /* handle options */

        if (s == "-bg") {
            if (i+1 == args.end()) {
                die("not enough parameters given");
            }
            m_bgColor = parseColorName(*(i+1));
            ++i;
            continue;
        }

        if (s == "-fg") {
            if (i+1 == args.end()) {
                die("not enough parameters given");
            }
            m_fgColor = parseColorName(*(i+1));
            ++i;
            continue;
        }
    }
}

unsigned long
Thingylaunch::parseColorName(std::string colorName)
{
    XColor tmp;
	XParseColor(m_display, DefaultColormap(m_display, m_screenNum), colorName.c_str(), &tmp);
	XAllocColor(m_display, DefaultColormap(m_display, m_screenNum), &tmp);
	return tmp.pixel;
}

void
Thingylaunch::setupGC()
{
    int valuemask = 0;
    int line_width = 1;
    int line_style = LineSolid;
    int cap_style = CapButt;
    int join_style = JoinBevel;
    XGCValues values;

    /* GC for text */
    m_gc = XCreateGC(m_display, m_win, valuemask, &values);
    XSetForeground(m_display, m_gc, m_fgColor);
    XSetBackground(m_display, m_gc, m_bgColor);
    XSetLineAttributes(m_display, m_gc, line_width, line_style, cap_style, join_style);
    if ((m_fontInfo = XLoadQueryFont(m_display, "-misc-*-medium-r-*-*-15-*-*-*-*-*-*-*")) == nullptr) {
        die("couldn't load font");
    }
    XSetFont(m_display, m_gc, m_fontInfo->fid);

    /* GC for rectangle */
    m_rectgc = XCreateGC(m_display, m_win, valuemask, &values);
    XSetForeground(m_display, m_rectgc, m_bgColor);
    XSetBackground(m_display, m_rectgc, m_bgColor);
}

void
Thingylaunch::grabHack()
{
#if 1
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 5000000;
    unsigned long maxwait = 3000000000UL; /* 3 seconds */
    unsigned int i;
    int x;

    redraw();

    /* this loop is required since pwm grabs the keyboard during the event loop */
    for (i = 0; i < (maxwait / req.tv_nsec); i++) {
        nanosleep(&req, NULL);
        x = XGrabKeyboard(m_display, m_win, False, GrabModeAsync, GrabModeAsync, CurrentTime);
        if (x == 0) {
            return;
        }
    }

    die("Couldn't grab keyboard");
#else
    XSetInputFocus(m_display, m_win, RevertToParent, CurrentTime);
#endif
}

void
Thingylaunch::eventLoop()
{
    XEvent e;

    redraw();

    XSelectInput(m_display, m_win, ExposureMask | KeyPressMask);

    for (;;) {
        XNextEvent(m_display, &e);
        switch(e.type) {
            case Expose:
                redraw();
                break;
            case KeyPress:
                if (keypress(&e.xkey)) {
                    return;
                }
                break;
            default:
                break;
        }
    }
}

void
Thingylaunch::redraw()
{
    int font_height = m_fontInfo->ascent + m_fontInfo->descent;
    int cursorLeft = XTextWidth(m_fontInfo, m_command.c_str(), m_cursorPos - m_command.begin());

    XFillRectangle(m_display, m_win, m_rectgc, 0, 0, WINWIDTH, WINHEIGHT);
    XDrawRectangle(m_display, m_win, m_gc, 0, 0, WINWIDTH-1, WINHEIGHT-1);
    XDrawString(m_display, m_win, m_gc, 2, font_height + 2, m_command.c_str(), m_command.size());
    XDrawLine(m_display, m_win, m_gc, 2 + cursorLeft, font_height + 4, 2 + cursorLeft + 10, font_height + 4);
    XFlush(m_display);
}

bool
Thingylaunch::keypress(XKeyEvent * keyevent)
{
    char charPressed;
    KeySym key_symbol;

    XLookupString(keyevent, &charPressed, 1, &key_symbol, NULL);

    /* check for an Alt-key meaning bookmark lookup */
    if (keyevent->state & Mod1Mask) {
        m_command = m_book.lookup(charPressed);
        if (!m_command.empty()) {
            m_hist.save(m_command);
            execcmd();
            return true;
        }
    }

    switch(key_symbol) {
        case XK_Escape:
            exit(0);
            break;

        case XK_BackSpace:
            m_comp.reset();
            if (m_cursorPos != m_command.begin())
                m_command.erase(--m_cursorPos);
            break;

        case XK_Left:
        case XK_KP_Left:
            if (m_cursorPos != m_command.begin())
                m_cursorPos--;
            break;

        case XK_Right:
        case XK_KP_Right:
            if (m_cursorPos < m_command.end())
                m_cursorPos++;
            break;

        case XK_Up:
        case XK_KP_Up:
            m_command = m_hist.prev();
            m_cursorPos = m_command.end();
            break;

        case XK_Down:
        case XK_KP_Down:
            m_command = m_hist.next();
            m_cursorPos = m_command.end();
            break;

        case XK_Home:
        case XK_KP_Home:
            m_cursorPos = m_command.begin();
            break;

        case XK_End:
        case XK_KP_End:
            m_cursorPos = m_command.end();
            break;

        case XK_Return:
            m_hist.save(m_command);
            execcmd();
            return true;
            break;

        case XK_Tab:
        case XK_KP_Tab:
            m_command = m_comp.next(m_command);
            m_cursorPos = m_command.end();
            break;

        case XK_k:
            if (keyevent->state & ControlMask) {
                m_comp.reset();
                m_command.clear();
                m_cursorPos = m_command.begin();
                key_symbol = 0; // don't handle the 'k' below
            }
            break;

        case XK_w:
            if (keyevent->state & ControlMask) {
                auto i = m_cursorPos - 1;
                while (i > m_command.begin()) {
                    if (*--i == ' ') {
                        break;
                    }
                }
                /* do not remove the heading space */
                if (i != m_command.begin()) {
                    ++i;
                }

                m_command.erase(i, m_cursorPos);

                m_cursorPos = i;
                key_symbol = 0; // don't handle the 'w' below
            }
            break;

        default:
            break;
    }

    /* normal printable chars including Latin-[1-8] + Keybad numbers */
    if ((key_symbol >= 0x20 && key_symbol <= 0x13be) || (key_symbol >= 0xffb0 && key_symbol <= 0xffb9)) {
        /* if we're not appending, shift the following characters */
        if (m_cursorPos == m_command.end()) {
            m_command.push_back(charPressed);
        } else {
            m_command.insert(m_cursorPos, charPressed);
        }
        ++m_cursorPos;
        m_comp.reset();
    }

    redraw();
    return false;
}

void
Thingylaunch::execcmd()
{
    const char * shell;
    const char * argv[4] { 0 };

    XUngrabKeyboard(m_display, CurrentTime);
    XDestroyWindow(m_display, m_win);

    if (fork()) {
        return;
    }

    shell = getenv("SHELL");
    if (!shell) {
        shell = "/bin/sh";
    }

    argv[0] = basename(shell);
    argv[1] = "-c";
    argv[2] = m_command.c_str();
    argv[3] = NULL;

    execv(shell, const_cast<char * const *>(argv));
    /* not reached */
}

void
Thingylaunch::die(std::string msg)
{
    std::cerr << "Error: " << msg << std::endl;
    exit(1);
}

/*
   Copyright (c) 2003 Matt Johnston
   All rights reserved.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
   */
