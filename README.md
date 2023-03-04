thingylaunch
============

Simple X11 application launcher.

This project is a complete rewrite of the original thinglaunch by Matt Johnston.

Thingylaunch has been enhanced with the following features:

* XCB backend
* tab-completion
* history navigation, with the UpArrow and DownArrow keys
* bookmarks, activated by `Alt+char`, loaded from the ~/.thingylaunch.bookmarks file, which consists of lines structured as `char command`
* command line arguments
```
   -fg    foreground color
   -bg    background color
   ⁻fo    font foundry
   -ff    font family
   -fw    font weight
   -fs    font slant
   -fwn   font width name
   -fsn   font style name
   -fps   font point size
   -x     window x-coordinate
   -y     window y-coordinate
   -w     window width
   -h     window height
```
