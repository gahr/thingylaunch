/*-
 * Copyright (C) 2003      Matt Johnston <matt@ucc.asn.au>
 * Copyright (C) 2009-2017 Pietro Cerutti <gahr@gahr.ch>
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

#include <X11/X.h>
#include <X11/keysym.h>

#include <libgen.h>
#include <unistd.h>

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <sstream>
using namespace std;

#include "bookmark.h"
#include "completion.h"
#include "history.h"
#include "util.h"
#include "x11_interface.h"

class Thingylaunch {

    public:
        Thingylaunch();
        ~Thingylaunch();

        void run(int argc, char **argv);

    private:
        bool readOptions(int argc, char **argv);
        void usage(const char * progname);
        void setupGC();
        void eventLoop();
        void grabKeyboard();
        bool keypress(X11Event& ev);
        void execcmd();
        void die(string msg);

        string parseFontDesc();

    private:

        /* X11 */
        X11Interface * m_x11;

        /* User-defined options */
        string m_fgColorName;
        string m_bgColorName;
        vector<string> m_fontDesc;

        /* Completion, history, and bookmarks */
        Completion m_comp;
        History    m_hist;
        Bookmark   m_book;

        /* The command */
        string m_command;
        string::size_type m_cursorPos;

        /* The window size */
        static constexpr int WindowWidth { 640 };
        static constexpr int WindowHeight { 25 };
};

Thingylaunch::Thingylaunch()
    : m_x11 { X11Interface::create() },
      m_fgColorName { "white" },
      m_bgColorName { "black" },
      m_fontDesc { "*", "*", "medium", "r", "*", "*", "15", "*", "*", "*", "*", "*", "*", "*" },
      m_cursorPos { 0 }
{ }

Thingylaunch::~Thingylaunch()
{
    delete m_x11;
}

string
Thingylaunch::parseFontDesc()
{
    stringstream ss;
    for (auto& i : m_fontDesc)
        ss << "-" << i;
    return ss.str();
}

void
Thingylaunch::run(int argc, char **argv)
{
    if (!readOptions(argc, argv)) {
        return;
    }

    if (!m_x11->createWindow(WindowWidth, WindowHeight)) {
        die("Couldn't open window");
    }

    if (!m_x11->setupGC(m_bgColorName, m_fgColorName, parseFontDesc())) {
        die("Couldn't setup GC");
    }

    if (!m_x11->grabKeyboard()) {
        die ("Couldn't grab keyboard");
    }

    eventLoop();
}

int
main(int argc, char **argv)
{
    Thingylaunch t;
    t.run(argc, argv);

    return 0;
}

bool
Thingylaunch::readOptions(int argc, char **argv)
{

#define setParam(varName) { \
    if (i+1 == end(args)) { \
        usage(argv[0]); \
        return false; \
    } \
    (varName) = *(i+1); \
    ++i; \
    continue; \
}

    vector<string> args;
    copy(argv+1, argv+argc, back_inserter(args));

    for (auto i = begin(args); i != end(args); ++i) {
        const auto& s = *i;

        /* background color */
        if (s == "-bg") {
            setParam(m_bgColorName);
        }

        /* foreground color */
        if (s == "-fg") {
            setParam(m_fgColorName);
        }

        /* font foundry */
        if (s == "-fo") {
            setParam(m_fontDesc[0]);
        }

        /* font family */
        if (s == "-ff") {
            setParam(m_fontDesc[1]);
        }

        /* font weight */
        if (s == "-fw") {
            setParam(m_fontDesc[2]);
        }

        /* font slant */
        if (s == "-fs") {
            setParam(m_fontDesc[3]);
        }

        /* font width name */
        if (s == "-fwn") {
            setParam(m_fontDesc[4]);
        }

        /* font style name */
        if (s == "-fsn") {
            setParam(m_fontDesc[5]);
        }

        /* font point size */
        if (s == "-fpt") {
            setParam(m_fontDesc[6]); 
        }

        usage(argv[0]);
        return false;
    }
    return true;
}

void
Thingylaunch::usage(const char * progname)
{
    std::cerr <<
        "Usage: " << progname << " "
        "[-bg background] "
        "[-fg foreground] "
        "[-fo font_foundry] "
        "[-ff font_family] "
        "[-fw font_weight] "
        "[-fw font_slant] "
        "[-fwn font_width_name] "
        "[-fsn font_style_name] "
        "[-fpt font_point_size]\n";
}

void
Thingylaunch::eventLoop()
{
    X11Event ev;

    if (!m_x11->redraw(m_command, m_cursorPos)) {
        die("Couldn't redraw");
    }

    while (m_x11->nextEvent(ev)) {

        switch (ev.type) {
            case X11Event::EventType::Evt_Expose:
                break;

            case X11Event::EventType::Evt_KeyPress:
                if (keypress(ev)) {
                    return;
                }
                break;

            case X11Event::EventType::Evt_Other:
                break;
        }

        if (!m_x11->redraw(m_command, m_cursorPos)) {
            die("Couldn't redraw");
        }
    }
}

bool
Thingylaunch::keypress(X11Event& ev)
{
    /* check for a Shift-key meaning capital letter */
    if (ev.state & ShiftMask) {
        ev.key  = toupper(ev.key);
    }

    /* check for an Alt-key meaning bookmark lookup */
    if (ev.state & Mod1Mask) {
        string book = m_book.lookup(ev.key);
        if (!book.empty()) {
            m_command = move(book);
            m_hist.save(m_command);
            execcmd();
            return true;
        }
    }

    switch(ev.key) {
        case XK_Escape:
            exit(0);
            break;

        case XK_BackSpace:
            m_comp.reset();
            if (m_cursorPos != 0)
                m_command.erase(--m_cursorPos);
            break;

        case XK_Left:
        case XK_KP_Left:
            if (m_cursorPos != 0)
                --m_cursorPos;
            break;

        case XK_Right:
        case XK_KP_Right:
            if (m_cursorPos < m_command.length())
                ++m_cursorPos;
            break;

        case XK_Up:
        case XK_KP_Up:
            m_command = m_hist.prev();
            m_cursorPos = m_command.length();
            break;

        case XK_Down:
        case XK_KP_Down:
            m_command = m_hist.next();
            m_cursorPos = m_command.length();
            break;

        case XK_Home:
        case XK_KP_Home:
            m_cursorPos = 0;
            break;

        case XK_End:
        case XK_KP_End:
            m_cursorPos = m_command.length();
            break;

        case XK_Return:
            m_hist.save(m_command);
            execcmd();
            return true;
            break;

        case XK_Tab:
        case XK_KP_Tab:
            m_command = m_comp.next(m_command);
            m_cursorPos = m_command.length();
            break;

        case XK_k:
            if (ev.state & ControlMask) {
                m_comp.reset();
                m_command.clear();
                m_cursorPos = 0;
                ev.key = 0; // don't handle the 'k' below
            }
            break;

        case XK_w:
            if (ev.state & ControlMask) {
                auto i = m_cursorPos - 1;
                while (i > 0) {
                    if (m_command[--i] == ' ') {
                        break;
                    }
                }
                /* do not remove the heading space */
                if (i != 0) {
                    ++i;
                }

                m_command.erase(i, m_cursorPos);

                m_cursorPos = i;
                ev.key = 0; // don't handle the 'w' below
            }
            break;

        default:
            break;
    }

    /* normal printable chars including Latin-[1-8] + Keybad numbers */
    if ((ev.key >= 0x20 && ev.key <= 0x13be) || (ev.key >= 0xffb0 && ev.key <= 0xffb9)) {
        if (m_cursorPos == m_command.length()) {
            m_command.push_back(ev.key);
        } else {
            m_command.insert(m_cursorPos, 1, ev.key);
        }
        ++m_cursorPos;
        m_comp.reset();
    }

    return false;
}

void
Thingylaunch::execcmd()
{
    if (fork()) {
        return;
    }

    string shell;
    try {
        shell = Util::getEnv("SHELL");
    } catch (...) {
        shell = "/bin/sh";
    }

    const char * argv[4] { 0 };
    argv[0] = basename(const_cast<char *>(shell.c_str()));
    argv[1] = "-c";
    argv[2] = m_command.c_str();
    argv[3] = NULL;

    execv(shell.c_str(), const_cast<char * const *>(argv));
    /* not reached */
}

void
Thingylaunch::die(string msg)
{
    cerr << "Error: " << msg << endl;
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
