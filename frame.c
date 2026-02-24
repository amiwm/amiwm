#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#ifdef HAVE_X11_EXTENSIONS_SHAPE_H
#include <X11/extensions/shape.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drawinfo.h"
#include "screen.h"
#include "menu.h"
#include "frame.h"
#include "icon.h"
#include "client.h"
#include "icc.h"
#include "prefs.h"
#include "module.h"
#include "libami.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

#define mx(a,b) ((a)>(b)?(a):(b))

extern Display *dpy;
extern XContext client_context, screen_context;
extern Cursor wm_curs;
extern int shape_extn;
void reshape_frame(Client *c);

Window creategadget(Client *c, Window p, int x, int y, int w, int h)
{
  XSetWindowAttributes attr;
  Window r;

  attr.override_redirect=True;
  attr.background_pixel=scr->dri.dri_Pens[BACKGROUNDPEN];
  r=XCreateWindow(dpy, p, x, y, w, h, 0, CopyFromParent, InputOutput,
		  CopyFromParent, CWOverrideRedirect|CWBackPixel, &attr);
  XSelectInput(dpy, r, ExposureMask|ButtonPressMask|ButtonReleaseMask|
	       EnterWindowMask|LeaveWindowMask);
  XSaveContext(dpy, r, client_context, (XPointer)c);
  return r;
}

void spread_top_gadgets(Client *c)
{
  int w=c->pwidth;
  w-=23; XMoveWindow(dpy, c->depth, w, 0);
  if(c->zoom) {
    w-=23; XMoveWindow(dpy, c->zoom, w, 0);
  }
  if(c->iconify) {
    w-=23; XMoveWindow(dpy, c->iconify, w, 0);
  }
  XResizeWindow(dpy, c->drag, c->dragw=mx(4, w-19), scr->bh);
}

void setclientborder(Client *c, int old, int new)
{
  int wc=0;
  int x=c->x, y=c->y, oldpw=c->pwidth, oldph=c->pheight;
  old&=(Psizeright|Psizebottom|Psizetrans);
  new&=(Psizeright|Psizebottom|Psizetrans);
  if(new!=old) {
    if((new & Psizeright)&&!(old & Psizeright))
      { c->pwidth+=14; wc++; }
    else if((old & Psizeright)&&!(new & Psizeright))
      { c->pwidth-=14; wc++; }
    if((new & Psizebottom)&&!(old & Psizebottom))
      c->pheight+=8;
    else if((old & Psizebottom)&&!(new & Psizebottom))
      c->pheight-=8;
    XResizeWindow(dpy, c->parent, c->pwidth, c->pheight);
    if(c->resize)
      XMoveWindow(dpy, c->resize, c->pwidth-18, c->pheight-10);
    if(wc) 
      spread_top_gadgets(c);
  }
  c->proto = (c->proto&~(Psizeright|Psizebottom|Psizetrans)) | new;
  c->framewidth=((new&Psizeright)?22:8);
  c->frameheight=scr->bh+((new&Psizebottom)?10:2);
  if(c->gravity==EastGravity || c->gravity==NorthEastGravity ||
     c->gravity==SouthEastGravity)
    x-=(c->pwidth-oldpw);
  if(c->gravity==SouthGravity || c->gravity==SouthEastGravity ||
     c->gravity==SouthWestGravity)
    y-=(c->pheight-oldph);
  if(x!=c->x || y!=c->y)
    XMoveWindow(dpy, c->parent, c->x=x, c->y=y);
}

void resizeclientwindow(Client *c, int width, int height)
{
  if(width!=c->pwidth || height!=c->pheight) {
    int old_width=c->pwidth;
    XResizeWindow(dpy, c->parent, c->pwidth=width, c->pheight=height);
    if(width!=old_width)
      spread_top_gadgets(c);
    if(c->resize)
      XMoveWindow(dpy, c->resize, c->pwidth-18, c->pheight-10);
    if (!c->fullscreen)
      XResizeWindow(dpy, c->window, c->pwidth-c->framewidth, c->pheight-c->frameheight);
    if(c->shaped)
      reshape_frame(c);
    sendconfig(c);
  }
}

static int resizable(XSizeHints *xsh)
{
  int r=0;
  if(xsh->width_inc) {
    if(xsh->min_width+xsh->width_inc<=xsh->max_width)
      r++;
  }
  if(xsh->height_inc) {
    if(xsh->min_height+xsh->height_inc<=xsh->max_height)
      r+=2;
  }
  return r;
}

extern unsigned int meta_mask;

void reshape_frame(Client *c)
{
  XRectangle r;

#ifdef HAVE_XSHAPE
  if(!shape_extn)
    return;

  XShapeCombineShape(dpy, c->parent, ShapeBounding, 4, c->scr->bh, c->window,
		     ShapeBounding, ShapeSet);

  r.x = 0;
  r.y = 0;
  r.width = c->pwidth;
  r.height = c->scr->bh;

  XShapeCombineRectangles(dpy, c->parent, ShapeBounding,
			  0, 0, &r, 1, ShapeUnion, Unsorted);

  if(c->proto & Psizeright) {
    r.x = c->pwidth-18;
    r.y = 0;
    r.width = 18;
    r.height = c->pheight;
    XShapeCombineRectangles(dpy, c->parent, ShapeBounding,
			    0, 0, &r, 1, ShapeUnion, Unsorted);
  }
  if(c->proto & Psizebottom) {
    r.x = 0;
    r.y = c->pheight-10;
    r.width = c->pwidth;
    r.height = 10;
    XShapeCombineRectangles(dpy, c->parent, ShapeBounding,
			    0, 0, &r, 1, ShapeUnion, Unsorted);
  }
#endif
}

void redrawclient(Client *);

void reparent(Client *c)
{
  XWindowAttributes attr;
  XSetWindowAttributes attr2;
  XTextProperty screen_prop;
  Scrn *s=scr;
  int cb;
  Window leader;
  Client *lc;
  struct mcmd_keygrab *kg;
  extern struct mcmd_keygrab *keygrabs;
  char **cargv = NULL;
  int cargc;
  int size_changed = 0;

  getwmstate(c);
  if(XGetTransientForHint(dpy, c->window, &leader) &&
     !XFindContext(dpy, leader, client_context, (XPointer *)&lc) &&
     lc != NULL) {
    c->leader = lc;
    if (lc->fsscr != NULL)
      c->scr = lc->fsscr;
    else
      c->scr = lc->scr;
  } else if(XGetTextProperty(dpy, c->window, &screen_prop, ATOMS[AMIWM_SCREEN])) {
    do {
      if(s->root == scr->root &&
	 (!s->deftitle[screen_prop.nitems])&&!strncmp(s->deftitle,
		      (char *)screen_prop.value, screen_prop.nitems))
	break;
      s=s->behind;
    } while(s!=scr);
    XFree(screen_prop.value);
    c->scr=s;
  } else if(XGetCommand(dpy, c->window, &cargv, &cargc)) {
    XrmDatabase db = (XrmDatabase)NULL;
    XrmValue value;
    char *str_type;
    static XrmOptionDescRec table [] = {
      {"-xrm", NULL, XrmoptionResArg, (caddr_t) NULL},
    };
    XrmParseCommand(&db, table, 1, "amiwm", &cargc, cargv);
    if(XrmGetResource(db, "amiwm.screen", "Amiwm.Screen",
		      &str_type, &value) == True &&
       value.size != 0) {
      do {
	if(s->root == scr->root &&
	   (!s->deftitle[value.size])&&!strncmp(s->deftitle, value.addr,
						value.size))
	  break;
	s=s->behind;
      } while(s!=scr);
      c->scr=s;
    }
    XrmDestroyDatabase (db);
  }
  scr=c->scr;
  if(c->parent && c->parent != scr->root)
    return;
  getproto(c);
  getwflags(c);
  XAddToSaveSet(dpy, c->window);
  XGetWindowAttributes(dpy, c->window, &attr);
  c->colormap = attr.colormap;
  c->old_bw = attr.border_width;
  c->framewidth=8;
  c->frameheight=scr->bh+2;
  attr2.override_redirect=True;


  grav_map_win_to_frame(c, attr.x, attr.y, &c->x, &c->y);

  /* Note: this is half of framewidth */
  if ((c->x < 0) || (c->x > scr->width)) {
    c->x = 0;
  }
  /* Note: this is the menu size, but no window framing */
  if ((c->y < scr->bh) || (c->y > scr->height)) {
    c->y = scr->bh;
  }

  /*
   * Check to make sure we're not created larger than the
   * available desktop size
   */
  if (attr.width > (scr->width - c->framewidth)) {
    attr.width = (scr->width - c->framewidth);
    size_changed = 1;
  }
  if (attr.height > (scr->height - c->frameheight)) {
    attr.height = (scr->height - c->frameheight);
    size_changed = 1;
  }

  c->pwidth = attr.width + c->framewidth;
  c->pheight = attr.height + c->frameheight;

  /*
   * Note: if we adjusted c->pwidth / c->pheight then
   * we need to call XResizeWindow on c->window.
   *
   * Do it before we reparent it just to make things
   * easy.
   */
  if (size_changed == 1) {
    XResizeWindow(dpy, c->window, c->pwidth-c->framewidth,
    c->pheight-c->frameheight);
  }

  /* Create the parent window that'll have our decoration */
  c->parent=XCreateWindow(dpy, scr->back, c->x, c->y,
			  c->pwidth, c->pheight,
			  0, CopyFromParent, InputOutput, CopyFromParent,
			  CWOverrideRedirect, &attr2);
  XSaveContext(dpy, c->parent, client_context, (XPointer)c);
  XSaveContext(dpy, c->parent, screen_context, (XPointer)c->scr);
  XSetWindowBackground(dpy, c->parent, scr->dri.dri_Pens[BACKGROUNDPEN]);
  XSetWindowBorderWidth(dpy, c->window, 0);

  /* Reparent time */
  XReparentWindow(dpy, c->window, c->parent, 4, scr->bh);
  XSelectInput(dpy, c->window, EnterWindowMask | LeaveWindowMask |
	       ColormapChangeMask | PropertyChangeMask |
	       (c->module? SubstructureNotifyMask:0));
#ifdef HAVE_XSHAPE
  if(shape_extn)
    XShapeSelectInput(dpy, c->window, ShapeNotifyMask);
#endif
  XSelectInput(dpy, c->parent, SubstructureRedirectMask |
	       SubstructureNotifyMask | StructureNotifyMask | ExposureMask |
	       EnterWindowMask | LeaveWindowMask | ButtonPressMask);
  for(kg=keygrabs; kg; kg=kg->next)
    XGrabKey(dpy, kg->keycode, kg->modifiers, c->window, False, GrabModeAsync,
	     GrabModeAsync);
  cb=(resizable(c->sizehints)? prefs.sizeborder:0);
  c->close=creategadget(c, c->parent, 0, 0, 19, scr->bh);
  c->drag=creategadget(c, c->parent, 19, 0, 1, 1);
  if(c->wflags&WF_NOICONIFY || c->leader != NULL) {
    c->iconify=None;
  }
  else
    c->iconify=creategadget(c, c->parent, 0, 0, 23, scr->bh);
  if(cb)
    c->zoom=creategadget(c, c->parent, 0, 0, 23, scr->bh);
  c->depth=creategadget(c, c->parent, 0, 0, 23, scr->bh);
  spread_top_gadgets(c);
  setclientborder(c, 0, cb);
  if(cb)
    c->resize=creategadget(c, c->parent, c->pwidth-18, c->pheight-10, 18, 10);
  c->shaped = 0;
#ifdef HAVE_XSHAPE
  if(shape_extn) {
    int xbs, ybs, cShaped, xcs, ycs;
    unsigned int wbs, hbs, wcs, hcs;
    XShapeQueryExtents (dpy, c->window, &c->shaped, &xbs, &ybs, &wbs, &hbs,
			&cShaped, &xcs, &ycs, &wcs, &hcs);
    if(c->shaped)
      reshape_frame(c);
  }
#endif
  XMapSubwindows(dpy, c->parent);
  sendconfig(c);
  if (scr->deftitle != NULL)
    setstringprop(c->window, ATOMS[AMIWM_SCREEN], scr->deftitle);
  if(prefs.focus == FOC_CLICKTOTYPE)
    XGrabButton(dpy, Button1, AnyModifier, c->parent, True,
		ButtonPressMask, GrabModeSync, GrabModeAsync, None, wm_curs);
}

void redraw(Client *c, Window w)
{
  if(w==c->parent) {
    int x, y;
    x=c->pwidth-((c->proto&Psizeright)?18:4);
    y=c->pheight-((c->proto&Psizebottom)?10:2);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHINEPEN]);
    XDrawLine(dpy, w, scr->gc, 0, 0, c->pwidth-1, 0);
    XDrawLine(dpy, w, scr->gc, 0, 0, 0, c->pheight-1);
    XDrawLine(dpy, w, scr->gc, 4, y, x, y);
    XDrawLine(dpy, w, scr->gc, x, scr->bh, x, y);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
    XDrawLine(dpy, w, scr->gc, 1, c->pheight-1, c->pwidth-1, c->pheight-1);
    XDrawLine(dpy, w, scr->gc, c->pwidth-1, 1, c->pwidth-1, c->pheight-1);
    XDrawLine(dpy, w, scr->gc, 3, scr->bh-1, 3, y);
    XDrawLine(dpy, w, scr->gc, 3, scr->bh-1, x, scr->bh-1);
  } else if(w==c->close) {
    if(c->active) {
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHINEPEN]);
      XFillRectangle(dpy, w, scr->gc, 7, scr->h3, 4, scr->h7-scr->h3);
    }
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
    XDrawRectangle(dpy, w, scr->gc, 7, scr->h3, 4, scr->h7-scr->h3);
    if(c->clicked==w) {
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
      XDrawLine(dpy, w, scr->gc, 0, 0, 18, 0);
      XDrawLine(dpy, w, scr->gc, 0, 0, 0, scr->bh-2);
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHINEPEN]);
      XDrawLine(dpy, w, scr->gc, 0, scr->bh-1, 18, scr->bh-1);
      XDrawLine(dpy, w, scr->gc, 18, 1, 18, scr->bh-1);
    } else {
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHINEPEN]);
      XDrawLine(dpy, w, scr->gc, 0, 0, 18, 0);
      XDrawLine(dpy, w, scr->gc, 0, 0, 0, scr->bh-1);
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
      XDrawLine(dpy, w, scr->gc, 1, scr->bh-1, 18, scr->bh-1);
      XDrawLine(dpy, w, scr->gc, 18, 1, 18, scr->bh-1);    
    }
  } else if(w==c->drag) {
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->active?FILLTEXTPEN:TEXTPEN]);
    XSetBackground(dpy, scr->gc, scr->dri.dri_Pens[c->active?FILLPEN:BACKGROUNDPEN]);
#ifdef USE_FONTSETS
    if(c->title)
      XmbDrawImageString(dpy, w, scr->dri.dri_FontSet, scr->gc,
			 11, 1+scr->dri.dri_Ascent,
			 c->title, strlen(c->title));
#else
    if(c->title.value)
      XDrawImageString(dpy, w, scr->gc, 11, 1+scr->dri.dri_Ascent,
		       (char *)c->title.value, c->title.nitems);
#endif
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHINEPEN]);
    XDrawLine(dpy, w, scr->gc, 0, 0, c->dragw-1, 0);
    XDrawLine(dpy, w, scr->gc, 0, 0, 0, scr->bh-2);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
    XDrawLine(dpy, w, scr->gc, 0, scr->bh-1, c->dragw-1, scr->bh-1);
    XDrawLine(dpy, w, scr->gc, c->dragw-1, 1, c->dragw-1, scr->bh-1);
  } else if(w==c->iconify) {
    if(c->active) {
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHINEPEN]);
      XFillRectangle(dpy, w, scr->gc, 7, scr->h8-4, 4, 2);
    }
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
    XDrawRectangle(dpy, w, scr->gc, 7, scr->h8-4, 4, 2);
    if(c->clicked==w) {
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->active? FILLPEN:BACKGROUNDPEN]);
      XDrawRectangle(dpy, w, scr->gc, 5, scr->h2, 12, scr->h8-scr->h2);
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
      XDrawRectangle(dpy, w, scr->gc, 5, scr->h8-(scr->bh>11? 7:6), 9, (scr->bh>11? 7:6));
    } else {
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->active? FILLPEN:BACKGROUNDPEN]);
      XDrawRectangle(dpy, w, scr->gc, 5, scr->h8-(scr->bh>11? 7:6), 9, (scr->bh>11? 7:6));
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
      XDrawRectangle(dpy, w, scr->gc, 5, scr->h2, 12, scr->h8-scr->h2);
    }
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->clicked==w?SHADOWPEN:SHINEPEN]);
    XDrawLine(dpy, w, scr->gc, 0, 0, 22, 0);
    XDrawLine(dpy, w, scr->gc, 0, 0, 0, scr->bh-2);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->clicked==w?SHINEPEN:SHADOWPEN]);
    XDrawLine(dpy, w, scr->gc, 0, scr->bh-1, 22, scr->bh-1);
    XDrawLine(dpy, w, scr->gc, 22, 0, 22, scr->bh-1);
  } else if(w==c->zoom) {
    if(c->active) {
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->clicked==w? SHINEPEN:FILLPEN]);
      XFillRectangle(dpy, w, scr->gc, 5, scr->h2, 12, scr->h8-scr->h2);
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->clicked==w? FILLPEN:SHINEPEN]);
      XFillRectangle(dpy, w, scr->gc, 6, scr->h2, 5, scr->h5-scr->h2);
    }
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
    XDrawRectangle(dpy, w, scr->gc, 6, scr->h2, 5, scr->h5-scr->h2);
    XDrawRectangle(dpy, w, scr->gc, 5, scr->h2, 7, scr->h5-scr->h2);
    XDrawRectangle(dpy, w, scr->gc, 5, scr->h2, 12, scr->h8-scr->h2);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->clicked==w? SHADOWPEN:SHINEPEN]);
    XDrawLine(dpy, w, scr->gc, 0, 0, 22, 0);
    XDrawLine(dpy, w, scr->gc, 0, 0, 0, scr->bh-2);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->clicked==w? SHINEPEN:SHADOWPEN]);
    XDrawLine(dpy, w, scr->gc, 0, scr->bh-1, 22, scr->bh-1);
    XDrawLine(dpy, w, scr->gc, 22, 0, 22, scr->bh-1);
  } else if(w==c->depth) {
    if(c->active) {
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[BACKGROUNDPEN]);
      XFillRectangle(dpy, w, scr->gc, 4, scr->h2, 10, scr->h6-scr->h2);
    }
    if(c->clicked!=w) {
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
      XDrawRectangle(dpy, w, scr->gc, 4, scr->h2, 10, scr->h6-scr->h2);
    }
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->active?SHINEPEN:BACKGROUNDPEN]);
    XFillRectangle(dpy, w, scr->gc, 8, scr->h4, 10, scr->h8-scr->h4);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
    XDrawRectangle(dpy, w, scr->gc, 8, scr->h4, 10, scr->h8-scr->h4);
    if(c->clicked==w)
      XDrawRectangle(dpy, w, scr->gc, 4, scr->h2, 10, scr->h6-scr->h2);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->clicked==w? SHADOWPEN:SHINEPEN]);
    XDrawLine(dpy, w, scr->gc, 0, 0, 22, 0);
    XDrawLine(dpy, w, scr->gc, 0, 0, 0, scr->bh-2);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[c->clicked==w? SHINEPEN:SHADOWPEN]);
    XDrawLine(dpy, w, scr->gc, 0, scr->bh-1, 22, scr->bh-1);
    XDrawLine(dpy, w, scr->gc, 22, 0, 22, scr->bh-1);
  } else if(w==c->resize) {
    static XPoint points[]={{4,6},{13,2},{14,2},{14,7},{4,7}};
    if(c->active) {
      XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHINEPEN]);
      XFillPolygon(dpy, w, scr->gc, points, sizeof(points)/sizeof(points[0]), Convex, CoordModeOrigin);
    }
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
    XDrawLines(dpy, w, scr->gc, points, sizeof(points)/sizeof(points[0]), CoordModeOrigin);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHINEPEN]);
    XDrawLine(dpy, w, scr->gc, 0, 0, 16, 0);
    XDrawLine(dpy, w, scr->gc, 0, 0, 0, 8);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
    XDrawLine(dpy, w, scr->gc, 0, 9, 17, 9);
    XDrawLine(dpy, w, scr->gc, 17, 0, 17, 9);
  }
}

void redrawclient(Client *c)
{
  unsigned long bgpix;

  scr=c->scr;
  if((!c->parent) || c->parent == scr->root)
    return;
  bgpix=scr->dri.dri_Pens[c->active?FILLPEN:BACKGROUNDPEN];
  XSetWindowBackground(dpy, c->parent, bgpix);
  XSetWindowBackground(dpy, c->close, bgpix);
  XSetWindowBackground(dpy, c->drag, bgpix);
  if(c->iconify)
    XSetWindowBackground(dpy, c->iconify, bgpix);
  if(c->zoom)
    XSetWindowBackground(dpy, c->zoom, bgpix);
  XSetWindowBackground(dpy, c->depth, bgpix);
  if(c->resize)
    XSetWindowBackground(dpy, c->resize, bgpix);
  XClearWindow(dpy, c->parent);
  XClearWindow(dpy, c->close);
  XClearWindow(dpy, c->drag);
  if(c->iconify)
    XClearWindow(dpy, c->iconify);
  if(c->zoom)
    XClearWindow(dpy, c->zoom);
  XClearWindow(dpy, c->depth);
  if(c->resize)
    XClearWindow(dpy, c->resize);
  redraw(c, c->parent);
  redraw(c, c->close);
  redraw(c, c->drag);
  if(c->iconify)
    redraw(c, c->iconify);
  if(c->zoom)
    redraw(c, c->zoom);
  redraw(c, c->depth);
  if(c->resize)
    redraw(c, c->resize);
}

static Client *topmostmappedclient(Window *children, unsigned int nchildren)
{
  int n;
  Client *c;
  for(n=nchildren-1; n>=0; --n)
    if((!XFindContext(dpy, children[n], client_context, (XPointer*)&c)) &&
       (children[n]==c->parent || children[n]==c->window) &&
       c->state==NormalState)
      return c;
  return NULL;
}

static Client *bottommostmappedclient(Window *children, unsigned int nchildren)
{
  int n;
  Client *c;
  for(n=0; n<nchildren; n++)
    if((!XFindContext(dpy, children[n], client_context, (XPointer*)&c)) &&
       children[n]==c->parent && c->state==NormalState)
      return c;
  return NULL;
}

void raiselowerclient(Client *c, int place)
{
  Window r,p,*children;
  unsigned int nchildren;
  if(place!=PlaceOnTop &&
     XQueryTree(dpy, scr->back, &r, &p, &children, &nchildren)) {
    if(place==PlaceOnBottom || topmostmappedclient(children, nchildren)==c) {
      Client *c2 = bottommostmappedclient(children, nchildren);
      if(c2 != NULL && c2 != c) {
	Window ws[2];
	ws[0]=c2->parent;
	ws[1]=c->parent;
	XRestackWindows(dpy, ws, 2);
      } else if(place!=PlaceOnBottom)
	XRaiseWindow(dpy, c->parent);
    } else
      XRaiseWindow(dpy, c->parent);
    if(children) XFree(children);
  } else XRaiseWindow(dpy, c->parent);
}

extern void setfocus(Window);
extern Client *activeclient;

/*
 * Lower the top most client.
 *
 * Update focus if required.
 */
void
lowertopmostclient(Scrn *scr)
{
	Window r, p, *children;
	unsigned int nchildren;
	Client *c_top, *c_bot;
	Window ws[2];

	/* Query the list of windows under the active screen */
	if (XQueryTree(dpy, scr->back, &r, &p, &children, &nchildren) == 0) {
		fprintf(stderr, "%s: couldn't fetch the window list\n", __func__);
		return;
	}

	/*
	 * Grab the top most client
	 */
	c_top = topmostmappedclient(children, nchildren);
	if (c_top == NULL) {
		fprintf(stderr, "%s: couldn't get the top most mapped client\n", __func__);
		return;
	}

	/*
	 * And the bottom most client.
	 */
	c_bot = bottommostmappedclient(children, nchildren);
	if (c_bot == NULL) {
		fprintf(stderr, "%s: couldn't get the bottom most mapped client\n", __func__);
		return;
	}

	/*
	 * If we're doing click-to-focus, mark the old top-most window
	 * as inactive; mark the new top-most window as active and has focus.
	 */
	if (prefs.focus == FOC_CLICKTOTYPE) {
		c_top->active = False;
		c_bot->active = True;
		activeclient = c_bot;
		redrawclient(c_bot);
		redrawclient(c_top);
		setfocus(c_bot->window);
	}

	/*
	 * Push this to the bottom of the stack.
	 */
	ws[0]=c_bot->parent;
	ws[1]=c_top->parent;
	XRestackWindows(dpy, ws, 2);

	/*
	 * Free the children list.
	 */
	if (children)
		XFree(children);
}

/*
 * Raise the bottom most client.
 *
 * Update focus if required.
 */
void
raisebottommostclient(Scrn *scr)
{
	Window r, p, *children;
	unsigned int nchildren;
	Client *c_top, *c_bot;

	/* Query the list of windows under the active screen */
	if (XQueryTree(dpy, scr->back, &r, &p, &children, &nchildren) == 0) {
		fprintf(stderr, "%s: couldn't fetch the window list\n", __func__);
		return;
	}

	/*
	 * Grab the top most client
	 */
	c_top = topmostmappedclient(children, nchildren);
	if (c_top == NULL) {
		fprintf(stderr, "%s: couldn't get the top most mapped client\n", __func__);
		return;
	}

	/*
	 * And the bottom most client.
	 */
	c_bot = bottommostmappedclient(children, nchildren);
	if (c_bot == NULL) {
		fprintf(stderr, "%s: couldn't get the bottom most mapped client\n", __func__);
		return;
	}

	/*
	 * If we're doing click-to-focus, mark the old top-most window
	 * as inactive; mark the new top-most window as active and has focus.
	 */
	if (prefs.focus == FOC_CLICKTOTYPE) {
		c_top->active = False;
		c_bot->active = True;
		activeclient = c_bot;
		redrawclient(c_bot);
		redrawclient(c_top);
		setfocus(c_bot->window);
	}

	/* Raise the selected window to the top */
	XRaiseWindow(dpy, c_bot->parent);

	/*
	 * Free the children list.
	 */
	if (children)
		XFree(children);
}

extern void get_drag_event(XEvent *event);

static Bool is_inside(int x0, int y0, int w, int h, int x, int y)
{
  if (w == 0 || h == 0)
    return True;  /* caller do not care about where click got released */
  return x >= x0 && y >= y0 && x < x0 + w && y < y0 + h;
}

enum click {
  CLICK_OUT = 0,        /* click failed or released outside button */
  CLICK_IN  = 1,        /* click released inside button */
  CLICK_ALT = 2,        /* click released inside button, while Shift pressed */
};

/* check if Button1 is pressed AND released with cursor inside window */
static enum click got_clicked(Client *c, Window w, Cursor curs, Time time)
{
  XWindowAttributes xwa;
  int status;

  XSync(dpy, False);
  if (!XGetWindowAttributes(dpy, w, &xwa))
    return CLICK_OUT;
  status = XGrabPointer(dpy, w, True, ButtonPressMask|ButtonReleaseMask,
                        GrabModeAsync, GrabModeAsync, False, curs, time);
  if (status != AlreadyGrabbed && status != GrabSuccess)
    return CLICK_OUT;
  c->clicked = w;
  redraw(c, w);
  for (;;) {
    XEvent event;

    get_drag_event(&event);
    if (event.type == ButtonRelease || event.type == ButtonPress) {
      c->clicked = None;
      redraw(c, w);
      XUngrabPointer(dpy, event.xbutton.time);
      if (event.type == ButtonPress)
        return CLICK_OUT;
      if (event.xbutton.button != Button1)
        return CLICK_OUT;
      if (!is_inside(0, 0, xwa.width, xwa.height, event.xbutton.x, event.xbutton.y))
        return CLICK_OUT;
      if (event.xbutton.button & ShiftMask)
        return CLICK_ALT;
      return CLICK_IN;
    }
  }
}

void click_close(Client *c, Time time)
{
  enum click click = got_clicked(c, c->close, None, time);
  if (click == CLICK_OUT)
    return;
  else if ((c->proto & Pdelete) && click != CLICK_ALT)
    sendcmessage(c->window, ATOMS[WM_PROTOCOLS], ATOMS[WM_DELETE_WINDOW]);
  else
    XKillClient(dpy, c->window);
}

void click_depth(Client *c, Time time)
{
  if (got_clicked(c, c->depth, None, time)) {
    raiselowerclient(c, -1);
  }
}

void click_zoom(Client *c, Time time)
{
  if (got_clicked(c, c->zoom, None, time)) {
    XWindowAttributes xwa;
    XGetWindowAttributes(dpy, c->parent, &xwa);
    XMoveWindow(dpy, c->parent, c->x=c->zoomx, c->y=c->zoomy);
    resizeclientwindow(c, c->zoomw+c->framewidth, c->zoomh+c->frameheight);
    c->zoomx=xwa.x;
    c->zoomy=xwa.y;
    c->zoomw=xwa.width-c->framewidth;
    c->zoomh=xwa.height-c->frameheight;
    sendconfig(c);
  }
}

void click_iconify(Client *c, Time time)
{
  if (got_clicked(c, c->iconify, None, time)) {
    iconify(c);
  }
}
