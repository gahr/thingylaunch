thingylaunch
============

Simple X11 application launcher.

The project is a fork of the original thinglaunch by Matt Johnston, available
at http://unix.freecode.com/projects/thinglaunch.

Thingylaunch has been enhanced with the following features:

 * tab-completion
 * history navigation, with the UpArrow and DownArrow keys
 * bookmarks support
   * loaded from the ~/.thingylaunch.bookmarks file, consisting of lines structured as follows:
   <pre>char command</pre>
   * activated by Alt+char
 * command line arguments
<pre>
   -fg    foreground color
   -bg    background color
   ⁻fo    font foundry
   -ff    font family
   -fw    font weight
   -fs    font slant
   -fwn   font width name
   -fsn   font style name
   -fps   font point size
</pre>

 * use either libX11 or libxcb, selected at build time using the CMake option
   <pre>-DUSE_XCB=ON</pre>

See also http://gahr.ch/thingylaunch/ .
