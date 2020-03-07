# sidplay2
fork of the sidplay2 source code to fix build on modern Linux systems.
See http://sidplay2.sourceforge.net/ for the original website.

Write to Dirk Jagdmann <doj@cubic.org> for questions about this fork.

## build

Install all the GNU autotools, C++ compiler and ALSA libaries.
Then simply run "./bootstrap" to build everything.

## information

This version id sidplay2 is source compatible with the libsidplay 2.1.1
and the new upcomming libsidplay 2.1.2.  Note these are not binary
compatible and as stated previously the 2.1.x serious is for development
to stabilise by 2.2.0.

Further changes to COM'ify the main interfaces are in the works.

Finally the use of the -q switch is depreciated.  Use -v with a
negative number instead (-q1 is equivalent to -v-1).
