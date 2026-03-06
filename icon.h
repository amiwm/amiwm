#ifndef ICON_H
#define ICON_H

#include "client.h"
#include "libami.h"

struct _Scrn;
typedef struct _Icon {
  struct _Icon *next, *nextselected;
  struct _Scrn *scr;
  Client *client;
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
extern void cleanupicons();
extern Icon *createappicon(struct module *, Window, char *, Pixmap, Pixmap, Pixmap, int, int);
extern void createdefaulticons();
extern void createicon(Client *);
extern void createiconicon(Icon *i, XWMHints *);
extern void deiconify(Icon *);
extern void deselect_all_icons(struct _Scrn *);
extern void deselecticon(Icon *);
extern void destroyiconicon(Icon *);
extern void free_icon_pms(struct IconPixmaps *pms);
extern void iconify(Client *);
extern void redrawicon(Icon *, Window);
extern void reparenticon(Icon *, struct _Scrn *, int, int);
extern void rmicon(Icon *);
extern void select_all_icons(struct _Scrn *i);
extern void selecticon(Icon *);

#endif
