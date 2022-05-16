#ifndef PREFS_H
#define PREFS_H

extern struct prefs_struct {
  int fastquit;
  int sizeborder;
  int forcemove;
  int borderwidth;
  int autoraise;
  int opaquemove;
  int opaqueresize;
  int customiconsonly;
  int shortlabelicons;
  char *icondir, *module_path, *defaulticon;
  int focus, manage_all, screenmenu;
  int titlebarclock;			/* display titlebar clock? */
  char *titleclockformat;		/* format to use for the clock */
  int titleclockinterval;		/* how often do we update the clock?*/
  int icontray;				// if true then icons will be shown in a tray on top of each screen (besides clock and screen name)
  int battery_info;			/* display battery info? */
  struct _Style *firststyle, *laststyle;
} prefs;

#define FM_MANUAL 0
#define FM_ALWAYS 1
#define FM_AUTO 2

#define FOC_FOLLOWMOUSE 0
#define FOC_CLICKTOTYPE 1
#define FOC_SLOPPY 2

#endif
