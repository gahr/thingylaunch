thingylaunch
============

Simple X11 application launcher.

The project is a fork of the original thinglaunch by Matt Johnston, available
at http://unix.freecode.com/projects/thinglaunch.

Thingylaunch has been enhanced with tab-completion, history navigation, and
bookmarks support. Moreover, it supports the -fg and -bg options to set the
foreground and background colors.

The history is browsed using the UpArrow and DownArrow keys. Auto-completion is
triggered by the Tab key. Bookmarks are loaded from the
~/.thingylaunch.bookmarks file, which consists of lines structured as follows:

    <char> <command>

Example:

    f firefox
    x xterm
    c gcalctool

Bookmarks are activated by Alt+char, e.g., Alt+f for firefox.

See also http://gahr.ch/thingylaunch/ .
