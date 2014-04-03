/* This program is a quick little launcher program for X.
 *
 * You run the program, it grabs the display (hopefully nothing else has it
 * grabbed), and you type what you want to run. The styling is minimalist.
 *
 * (c) 2009-2014 Pietro Cerutti
 * gahr (at) gahr.ch
 *
 * (c) 2003 Matt Johnston
 * matt (at) ucc.asn.au
 *
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysymdef.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

#include "completion.h"
#include "history.h"

static void createWindow();
static void eventLoop();
static void grabHack();
static void redraw();
static void keypress(XKeyEvent * keyevent);
static void execcmd();
static void die(const char * message);

Display * display;
GC gc;
GC rectgc;
Window win;
XFontStruct * font_info;
int screen_num;

comp_t comp;
hist_t hist;

#define WINWIDTH 640
#define WINHEIGHT 25

/* the actual commandline */
char command[MAX_CMD_LEN+1];
size_t cursor_pos;

int
main(void)
{
	cursor_pos = 0;

	if (!(comp = comp_init())) {
		exit(1);
	}

	if (!(hist = hist_init())) {
		exit (1);
	}

	createWindow();

	grabHack();

	eventLoop();

	/* don't return */
	return 1; /* keep compilers happy */
}

static void
createWindow()
{
	char * display_name;
	char * font_name;
	int display_width, display_height;
	int top, left;
	int valuemask = 0;
	int line_width = 1;
	int line_style = LineSolid;
	int cap_style = CapButt;
	int join_style = JoinBevel;
	unsigned long black, white;
	XSizeHints * win_size_hints;
	XSetWindowAttributes attrib;
	XGCValues values;

	display_name = getenv("DISPLAY");
	if (display_name == NULL) {
		die("DISPLAY not set");
	}

	display = XOpenDisplay(display_name);
	if (display == NULL) {
		die("Couldn't connect to DISPLAY");
	}

	screen_num = DefaultScreen(display);
	display_width = DisplayWidth(display, screen_num);
	display_height = DisplayHeight(display, screen_num);

	top = (display_height/2 - WINHEIGHT/2);
	left = (display_width/2 - WINWIDTH/2);

	black = BlackPixel(display, screen_num);
	white = WhitePixel(display, screen_num);

	attrib.override_redirect= True;
	win = XCreateWindow(display, RootWindow(display, screen_num),
			left, top, WINWIDTH, WINHEIGHT, 
			0, CopyFromParent,InputOutput,CopyFromParent,
			CWOverrideRedirect,&attrib);

	/* set up the window hints etc */
	win_size_hints = XAllocSizeHints();
	if (!win_size_hints) {
		die("out of memory allocating hints");
	}

	win_size_hints->flags = PMaxSize | PMinSize;
	win_size_hints->min_width = win_size_hints->max_width = WINWIDTH;

	win_size_hints->min_height = win_size_hints->max_height = WINHEIGHT;
	XSetWMNormalHints(display, win, win_size_hints);

	XFree(win_size_hints);

	/* show window */
	XMapWindow(display, win);

	/* setup GC */
	gc = XCreateGC(display, win, valuemask, &values);
	rectgc = XCreateGC(display, win, valuemask, &values);
	XSetForeground(display, gc, white);
	XSetBackground(display, gc, black);

	XSetForeground(display, rectgc, black);
	XSetBackground(display, rectgc, black);

	XSetLineAttributes(display, gc, line_width, line_style, 
			cap_style, join_style);

	/* setup the font */
	font_name = "-misc-*-medium-r-*-*-15-*-*-*-*-*-*-*";
	font_info = XLoadQueryFont(display, font_name);
	if (!font_info) {
		die("couldn't load font");
	}

	XSetFont(display, gc, font_info->fid);
}

static void
eventLoop()
{
	XEvent e;

	redraw();

	XSelectInput(display, win, ExposureMask | KeyPressMask);

	for (;;) {
		XNextEvent(display, &e);
		switch(e.type) {
			case Expose:
				redraw();
				break;
			case KeyPress:
				keypress(&e.xkey);
				break;
			default:
				break;
		}

	}
}

/* this loop is required since pwm grabs the keyboard during the event loop */
static void
grabHack()
{
	struct timespec req;
	req.tv_sec = 0;
	req.tv_nsec = 5000000;
	unsigned long maxwait = 3000000000UL; /* 3 seconds */
	unsigned int i;
	int x;

	redraw();

	for (i = 0; i < (maxwait / req.tv_nsec); i++) {
		nanosleep(&req, NULL);
		x = XGrabKeyboard(display, win, False, GrabModeAsync, GrabModeAsync, CurrentTime);
		if (x == 0) {
			return;
		}
	}

	die("Couldn't grab keyboard");
}

static void
redraw()
{
	int font_height;
	int textwidth;

	font_height = font_info->ascent + font_info->descent;
	textwidth = XTextWidth(font_info, command, cursor_pos);

	XFillRectangle(display, win, rectgc, 0, 0, WINWIDTH, WINHEIGHT);
	XDrawRectangle(display, win, gc, 0, 0, WINWIDTH-1, WINHEIGHT-1);
	XDrawString(display, win, gc, 2, font_height+2, command, strlen(command));
	XDrawLine(display, win, gc, 2 + textwidth, font_height + 4,
			2 + textwidth + 10, font_height+4);

	XFlush(display);
}

static void
keypress(XKeyEvent * keyevent)
{
#define KEYBUFLEN 20
	char buffer[KEYBUFLEN+1];
	char * cmd;
	KeySym key_symbol;
	size_t len, tmp_pos;

	len = XLookupString(keyevent, buffer, 1, &key_symbol, NULL);
	buffer[len] = '\0';
	len = strlen(command);

	switch(key_symbol) {
		case XK_Escape:
			comp_cleanup(comp);
			hist_cleanup(hist);
			exit(0);
			break;

		case XK_BackSpace:
			comp_reset(comp);
			if (cursor_pos)
				for (tmp_pos = --cursor_pos; tmp_pos <= len; tmp_pos++)
					command[tmp_pos] = command[tmp_pos+1];
			break;

		case XK_Left:
		case XK_KP_Left:
			if (cursor_pos)
				cursor_pos--;
			break;

		case XK_Right:
		case XK_KP_Right:
			if (cursor_pos < len)
				cursor_pos++;
			break;

		case XK_Up:
		case XK_KP_Up:
			cmd = hist_prev(hist);
			if (cmd)
				snprintf(command, MAX_CMD_LEN, "%s", cmd);
			else
				command[0] = '\0';
			cursor_pos = strlen(command);
			break;

		case XK_Down:
		case XK_KP_Down:
			cmd = hist_next(hist);
			if (cmd)
				snprintf(command, MAX_CMD_LEN, "%s", cmd);
			else
				command[0] = '\0';
			cursor_pos = strlen(command);
			break;

		case XK_Home:
		case XK_KP_Home:
			cursor_pos = 0;
			break;

		case XK_End:
		case XK_KP_End:
			cursor_pos = len;
			break;

		case XK_Return:
			hist_save(hist, command);
			execcmd();
			break;

		case XK_Tab:
		case XK_KP_Tab:
			cmd = comp_next(comp, command);
			if (!cmd) {
				break;
			}
			cmd = strrchr(cmd, '/');
			snprintf(command, MAX_CMD_LEN, "%s", cmd+1);
			cursor_pos = strlen(cmd+1);
			break;

		case XK_k:
			if (keyevent->state & ControlMask) {
				comp_reset(comp);
				memset(command, '\0', MAX_CMD_LEN);
				cursor_pos = 0;
				key_symbol = 0; // don't handle the 'k' below
			}
			break;

		case XK_w:
			if (keyevent->state & ControlMask) {
				while (cursor_pos) {
					for (tmp_pos = --cursor_pos; tmp_pos <= len; tmp_pos++) {
						command[tmp_pos] = command[tmp_pos+1];
					}
					if (command[cursor_pos-1] == ' ') {
						break;
					}
				}
				key_symbol = 0; // don't handle the 'w' below
			}
			break;

		default:
			break;
	}

	/* normal printable chars including Latin-[1-8] + Keybad numbers */
	if ((key_symbol >= 0x20 && key_symbol <= 0x13be) || (key_symbol >= 0xffb0 && key_symbol <= 0xffb9)) {
		if (len < MAX_CMD_LEN) {
			/* if we're not appending, shift the following characters */
			if (cursor_pos != len) {
				memmove(&command[cursor_pos+1], &command[cursor_pos], len-cursor_pos);
			}
			command[cursor_pos++] = buffer[0];
			command[len+1] = '\0';
			comp_reset(comp);
		}
	}

	redraw();
}

static void
execcmd()
{
    command[cursor_pos] = '\0';
	char * shell;
	char * argv[4];

	XDestroyWindow(display, win);

	if (fork()) {
		exit(0);
	}

	shell = getenv("SHELL");
	if (!shell) {
		shell = "/bin/sh";
	}

	argv[0] = basename(shell);
	argv[1] = "-c";
	argv[2] = command;
	argv[3] = NULL;

	execv(shell, argv);
	die("aiee, after exec");

}

static void
die(const char * msg) {

	fprintf(stderr, "Error: %s\n", msg);
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
