#ifndef ICON_H
#define ICON_H

#include <X11/Xutil.h>

#include "libami.h"

struct _Scrn;
struct _Client;
struct module;

typedef struct _Icon {
  struct _Icon *next, *nextselected;
  struct _Scrn *scr;
  struct _Client *client;
  struct module *module;
  Window parent, window, labelwin, innerwin;
  Pixmap iconpm, secondpm, maskpm;
#ifdef USE_FONTSETS
  char *label;
#else
  XTextProperty label;
#endif
  int x, y, width, height;
  int labelwidth;
  int selected, mapped;
} Icon;

struct IconPixmaps
{
  Pixmap pm, pm2;
  struct ColorStore cs, cs2;
};

extern void adjusticon(Icon *);
extern void cleanupicons(void);
extern void createdefaulticons(void);
extern void createicon(struct _Client *);
extern void createiconicon(Icon *i, XWMHints *);
extern void deiconify(Icon *);
extern void deselect_all_icons(struct _Scrn *);
extern void deselecticon(Icon *);
extern void destroyiconicon(Icon *);
extern void free_icon_pms(struct IconPixmaps *pms);
extern void iconify(struct _Client *);
extern void newicontitle(struct _Client *);
extern void redrawicon(Icon *, Window);
extern void reparenticon(Icon *, struct _Scrn *, int, int);
extern void rmicon(Icon *);
extern void select_all_icons(struct _Scrn *i);
extern void selecticon(Icon *);
extern Icon *createappicon(struct module *m, Window p, char *name,
                           Pixmap pm1, Pixmap pm2, Pixmap pmm, int x, int y);

#endif
