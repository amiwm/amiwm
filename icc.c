#include <X11/Xatom.h>

#include "drawinfo.h"
#include "screen.h"
#include "frame.h"
#include "icc.h"
#include "icon.h"
#include "style.h"
#include "prefs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

Atom ATOMS[NATOMS];

extern Display *dpy;
extern char *progname;

void init_atoms(void)
{
  static char *atom_names[NATOMS] = {
#define X(atom) [atom] = #atom,
    ATOMS_TABLE(X)
#undef  X
  };

  if (!XInternAtoms(dpy, atom_names, NATOMS, False, ATOMS)) {
    fprintf(stderr, "%s: cannot intern atoms\n", progname);
    exit(1);
  }
}

void setsupports(Window root, Window checkwin)
{
  XChangeProperty(dpy, root, ATOMS[_NET_SUPPORTED], XA_ATOM, 32,
    PropModeReplace, (void *)&ATOMS[_NET_SUPPORTED], NATOMS-_NET_SUPPORTED);
  XChangeProperty(dpy, root, ATOMS[_NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32,
    PropModeReplace, (void *)(Window[]){checkwin}, 1);
  XChangeProperty(dpy, checkwin, ATOMS[_NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32,
    PropModeReplace, (void *)(Window[]){checkwin}, 1);
  XChangeProperty(dpy, checkwin, ATOMS[_NET_WM_NAME], ATOMS[UTF8_STRING], 8,
    PropModeReplace, (void *)"AmiWM", 5);
}

void setstringprop(Window w, Atom a, char *str)
{
    XTextProperty txtp;

    txtp.value=(unsigned char *)str;
    txtp.encoding=XA_STRING;
    txtp.format=8;
    txtp.nitems=strlen(str);
    XSetTextProperty(dpy, w, &txtp, a);
}

void getwmstate(Client *c)
{
  Atom *p;
  int i;
  long n;
  Window w;

  w = c->window;
  if ((n = _getprop(w, ATOMS[_NET_WM_STATE], XA_ATOM, 20L, (char**)&p)) < 0)
    return;
  c->fullscreen = 0;
  if (!n)
    return;
  for (i = 0; i < n; i++)
    if (p[i] == ATOMS[_NET_WM_STATE_FULLSCREEN])
      c->fullscreen = 1;
  XFree((char *) p);
}

void setwmstate(Client *c)
{
  if (c->fullscreen) {
    XChangeProperty(dpy, c->window, ATOMS[_NET_WM_STATE], XA_ATOM, 32, PropModeReplace,
                   (void *)&ATOMS[_NET_WM_STATE_FULLSCREEN], 1);
  } else
    XDeleteProperty(dpy, c->window, ATOMS[_NET_WM_STATE]);
}

void sendcmessage(Window w, Atom a, long x)
{
  if(!(XSendEvent(dpy, w, False, 0L, mkcmessage(w, a, x))))
    XBell(dpy, 100);
}

long _getprop(Window w, Atom a, Atom type, long len, char **p)
{
  Atom real_type;
  int format;
  unsigned long n, extra;
  int status;
  
  status = XGetWindowProperty(dpy, w, a, 0L, len, False, type, &real_type, &format,
			      &n, &extra, (unsigned char **)p);
  if (status != Success || *p == 0)
    return -1;
  if (n == 0)
    XFree((void*) *p);
  return n;
}

void getwflags(Client *c)
{
  BITS32 *p;
  long n;

  c->wflags = 0;
  if ((n = _getprop(c->window, ATOMS[AMIWM_WFLAGS], ATOMS[AMIWM_WFLAGS], 1L, (char**)&p)) <= 0)
    return;

  c->wflags = p[0];
  
  XFree((char *) p);
}

void getproto(Client *c)
{
  Atom *p;
  int i;
  long n;
  Window w;

  w = c->window;
  c->proto &= ~(Pdelete|Ptakefocus);
  if ((n = _getprop(w, ATOMS[WM_PROTOCOLS], XA_ATOM, 20L, (char**)&p)) <= 0)
    return;

  for (i = 0; i < n; i++)
    if (p[i] == ATOMS[WM_DELETE_WINDOW])
      c->proto |= Pdelete;
    else if (p[i] == ATOMS[WM_TAKE_FOCUS])
      c->proto |= Ptakefocus;
  
  XFree((char *) p);
}

static int stylematch_low(char *p, int l, char *m)
{
  char *lf;
  int ml;
  --m;
  do {
    lf = strchr(++m, '\n');
    ml = (lf? lf-m:strlen(m));
    if(ml == l && !strncmp(m, p, ml))
      return 1;
  } while((m=lf));
  return 0;
}

static int stylematch_tprop(XTextProperty *p, char *m)
{
  return stylematch_low((char *)p->value, p->nitems, m);
}

#ifdef USE_FONTSETS
static int stylematch_str(char *p, char *m)
{
  return stylematch_low(p, strlen(p), m);
}
#endif

void checkstyle(Client *c)
{
  XTextProperty icon_name, class_name;
  Style *style;

  if(prefs.firststyle==NULL)
    return;

  if(!XGetTextProperty(dpy, c->window, &class_name, XA_WM_CLASS))
    class_name.value=NULL;
  else
    /* This value seems to be 2x it's correct value always... */
    class_name.nitems=strlen((char *)class_name.value);
  if(!XGetWMIconName(dpy, c->window, &icon_name))
    icon_name.value=NULL;

  for(style=prefs.firststyle; style!=NULL; style=style->next)
    if((class_name.value!=NULL && style->style_class!=NULL &&
	stylematch_tprop(&class_name, style->style_class)) ||
#ifdef USE_FONTSETS
       (c->title!=NULL && style->style_title!=NULL &&
	stylematch_str(c->title, style->style_title)) ||
#else
       (c->title.value!=NULL && style->style_title!=NULL &&
	stylematch_tprop(&c->title, style->style_title)) ||
#endif
       (icon_name.value!=NULL && style->style_icon_title!=NULL &&
	stylematch_tprop(&icon_name, style->style_icon_title))) {
      c->style = style;
      break;
    }

  if(icon_name.value)
    XFree(icon_name.value);
  if(class_name.value)
    XFree(class_name.value);
}

void propertychange(Client *c, Atom a)
{
  extern void checksizehints(Client *);
  extern void newicontitle(Client *);

  if(a==XA_WM_NAME) {
#ifdef USE_FONTSETS
    XTextProperty prop;
    if(c->title) {
      free(c->title);
      c->title = NULL;
    }
    if(XGetWMName(dpy, c->window, &prop) && prop.value) {
      char **list;
      int n;
      if(XmbTextPropertyToTextList(dpy, &prop, &list, &n) >= Success) {
	if(n > 0)
	  c->title = strdup(list[0]);
	XFreeStringList(list);
      }
      XFree(prop.value);
    }
#else
    if(c->title.value)
      XFree(c->title.value);
    XGetWMName(dpy, c->window, &c->title);
#endif
    if(c->style==NULL)
      checkstyle(c);
    if(c->drag) {
      XClearWindow(dpy, c->drag);
      redraw(c, c->drag);
    }
  } else if(a==XA_WM_NORMAL_HINTS) {
    checksizehints(c);
  } else if(a==XA_WM_HINTS) {
    XWMHints *xwmh;
    if((xwmh=XGetWMHints(dpy, c->window))) {
      if((xwmh->flags&(IconWindowHint|IconPixmapHint))&&c->icon) {
	destroyiconicon(c->icon);
	createiconicon(c->icon, xwmh);
      }
      if((xwmh->flags&IconPositionHint)&&c->icon&&c->icon->window) {
	XMoveWindow(dpy, c->icon->window, c->icon->x=xwmh->icon_x,
		    c->icon->y=xwmh->icon_y);
	adjusticon(c->icon);
      }
      XFree(xwmh);
    }
  } else if(a==ATOMS[WM_PROTOCOLS]) {
    getproto(c);
  } else if(a==XA_WM_ICON_NAME) {
    if(c->style==NULL)
      checkstyle(c);
    if(c->icon) newicontitle(c);
  } else if(a==ATOMS[WM_STATE]) {
    if(c->parent==c->scr->root) {
      getstate(c);
      if(c->state==NormalState)
	c->state=WithdrawnState;
    }
  } else if(a==XA_WM_CLASS && c->style==NULL)
    checkstyle(c);
}

void handle_client_message(Client *c, XClientMessageEvent *xcme)
{
  if(xcme->message_type == ATOMS[WM_CHANGE_STATE]) {
    int state=xcme->data.l[0];
    if(state==IconicState)
      if(c->state!=IconicState) {
	if(!(c->icon))
	  createicon(c);
	XUnmapWindow(dpy, c->parent);
	/*	XUnmapWindow(dpy, c->window); */
	adjusticon(c->icon);
	XMapWindow(dpy, c->icon->window);
	if(c->icon->labelwidth)
	  XMapWindow(dpy, c->icon->labelwin);
	c->icon->mapped=1;
	setclientstate(c, IconicState);
      } else ;
    else
      if(c->state==IconicState && c->icon) {
	Icon *i=c->icon;
	if(i->labelwin)
	  XUnmapWindow(dpy, i->labelwin);
	if(i->window)
	  XUnmapWindow(dpy, i->window);
	i->mapped=0;
	deselecticon(i);
	XMapWindow(dpy, c->window);
	if(c->parent!=c->scr->root && !c->fullscreen)
	  XMapRaised(dpy, c->parent);
	setclientstate(c, NormalState);
      }
  } else if (xcme->message_type == ATOMS[_NET_WM_STATE]) {
    int action=xcme->data.l[0];
    Atom prop=xcme->data.l[1];
    if (prop == ATOMS[_NET_WM_STATE_FULLSCREEN])
      switch (action) {
      case 0: fullscreen(c, 0); break; /* _NET_WM_STATE_REMOVE */
      case 1: fullscreen(c, 1); break; /* _NET_WM_STATE_ADD */
      case 2: fullscreen(c, !c->fullscreen); break; /* _NET_WM_STATE_TOGGLE */
      }
  }
}

Window get_transient_for(Window w)
{
  Window transient_for = None;

  if (!XGetTransientForHint(dpy, w, &transient_for))
    return None;
  return transient_for;
}
