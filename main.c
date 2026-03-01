#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/Xresource.h>
#include <X11/keysym.h>
#include <X11/Xmu/Error.h>
#ifdef HAVE_X11_EXTENSIONS_SHAPE_H
#include <X11/extensions/shape.h>
#endif
#ifdef AMIGAOS
#include <x11/xtrans.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef USE_FONTSETS
#include <locale.h>
#endif

#include "client.h"
#include "drawinfo.h"
#include "events.h"
#include "frame.h"
#include "icc.h"
#include "icon.h"
#include "libami.h"
#include "menu.h"
#include "module.h"
#include "prefs.h"
#include "rc.h"
#include "screen.h"


#define HYSTERESIS 5

Display *dpy = NULL;
Client *activeclient=NULL;
Bool shape_extn=False;
char *x_server=NULL;
XContext client_context, screen_context, icon_context, menu_context, vroot_context;
char *progname;
Cursor wm_curs;

static int signalled=0, forcemoving=0;
static Time last_icon_click=0, last_double=0;
static Client *doubleclient=NULL;
static int initting=0;
static int ignore_badwindow=0;
static int dblClickTime=1500;
static Window *checkwins;
static int shape_event_base, shape_error_base;
static int server_grabs=0;

static char **main_argv;

#ifndef AMIGAOS
void restart_amiwm()
{
  flushmodules();
  flushclients();
  XFlush(dpy);
  XCloseDisplay(dpy);
  execvp(main_argv[0], main_argv);
}
#endif

static int handler(Display *d, XErrorEvent *e)
{
  if (initting && (e->request_code == X_ChangeWindowAttributes) &&
      (e->error_code == BadAccess)) {
    fprintf(stderr, "%s: Another window manager is already running.  Not started.\n", progname);
    exit(1);
  }
  
  if (ignore_badwindow &&
      (e->error_code == BadWindow || e->error_code == BadColor))
    return 0;

  if ((e->error_code == BadMatch && e->request_code == X_ChangeSaveSet) ||
      (e->error_code == BadWindow && e->request_code == X_ChangeProperty) ||
      (e->error_code == BadWindow && e->request_code == X_GetProperty) ||
      (e->error_code == BadWindow && e->request_code == X_GetWindowAttributes) ||
      (e->error_code == BadWindow && e->request_code == X_ChangeWindowAttributes) ||
      (e->error_code == BadDrawable && e->request_code == X_GetGeometry) ||
      (e->error_code == BadWindow && e->request_code == X_SendEvent))
    return 0;
  
  XmuPrintDefaultErrorMessage(d, e, stderr);
  
  if (initting) {
    fprintf(stderr, "%s: failure during initialisation; aborting\n",
	    progname);
    exit(1);
  }
  return 0;
}

void setfocus(Window w)
{
  if(w == None && prefs.focus != FOC_CLICKTOTYPE)
    w = PointerRoot;
  XSetInputFocus(dpy, w, (prefs.focus==FOC_CLICKTOTYPE? RevertToNone:RevertToPointerRoot), CurrentTime);
}

static void update_clock(void *dontcare);

static void grab_server()
{
  if(!server_grabs++)
    XGrabServer(dpy);
}

static void ungrab_server()
{
  if(!--server_grabs) {
    XUngrabServer(dpy);
    if(prefs.titlebarclock) {
      remove_call_out(update_clock, NULL);
      update_clock(NULL);
    }
  }
}

struct bounding {
  Scrn *scr;
  int x, y, w, h;
  int d_offset;
};

static void draw_bounding(struct bounding *bounding)
{
  int x = bounding->x;
  int y = bounding->y;
  int w = bounding->w;
  int h = bounding->h;

  if (w < 0) {
    x += w;
    w *= -1;
  }
  if (h < 0) {
    y += h;
    h *= -1;
  }
  if (w >= HYSTERESIS || h >=HYSTERESIS) {
    XSetDashes(dpy, bounding->scr->rubbergc, bounding->d_offset,
               (char[]){6}, 1);
    XSetLineAttributes(dpy, bounding->scr->rubbergc, 0, LineOnOffDash,
                       CapButt, JoinMiter);
    XDrawRectangle(dpy, bounding->scr->back, bounding->scr->rubbergc,
                   x, y, w, h);
    XSetLineAttributes(dpy, bounding->scr->rubbergc, 0, LineSolid,
                       CapButt, JoinMiter);
  }
}

static void move_dashes(void *ctx)
{
  struct bounding *bounding = ctx;

  call_out(0, 50000, move_dashes, bounding);
  draw_bounding(bounding);
  if (--bounding->d_offset < 0)
    bounding->d_offset = 11;
  draw_bounding(bounding);
}

static void drag_bounding(Scrn *s, Time time, int x_root, int y_root, int x, int y)
{
  struct bounding bounding = {
    .scr = s,
    .x = x,
    .y = y,
    .w = 0,
    .h = 0,
    .d_offset = 0,
  };

  if (!grab_for_motion(s->back, None, None, time, x_root, y_root))
    return;
  grab_server();
  call_out(0, 0, move_dashes, &bounding);
  for (;;) {
    XEvent event;
    event = get_drag_event();
    draw_bounding(&bounding);
    if (event.type == ButtonRelease && event.xbutton.button == Button1) {
      remove_call_out(move_dashes, &bounding);
      if (!(event.xbutton.state & ShiftMask))
        deselect_all_icons(s);
      if (bounding.w < 0) {
        bounding.x += bounding.w;
        bounding.w *= -1;
      }
      if (bounding.w < 0) {
        bounding.y += bounding.h;
        bounding.h *= -1;
      }
      if (bounding.w >= HYSTERESIS || bounding.h >= HYSTERESIS) {
        for(Icon *i = s->icons; i != NULL; i = i->next) {
          int bx, by;
          if(i->window && i->mapped &&
             XTranslateCoordinates(dpy, i->parent, s->back, i->x, i->y,
                                   &bx, &by, &(Window){0}) &&
             bx < bounding.x + bounding.w && bx + i->width>bounding.x &&
             by < bounding.y + bounding.h && by + i->height>bounding.y) {
            selecticon(i);
          }
        }
      }
      break;
    }
    if (event.type == ButtonPress && event.xbutton.button == Button3)
      break;
    if (event.type != MotionNotify)
      continue;
    bounding.w = event.xmotion.x_root - x_root;
    bounding.h = event.xmotion.y_root - y_root;
    draw_bounding(&bounding);
  }

  ungrab_server();
  XUngrabPointer(dpy, CurrentTime);
}

static void drag_move(Client *c, Time time, int x_root, int y_root)
{
  int x = c->x;
  int y = c->y;
  int dx = x_root - c->x;
  int dy = y_root - c->y;

  if (!grab_for_motion(c->drag, c->scr->back, None, time, x_root, y_root))
    return;
  grab_server();
  if(!prefs.opaquemove) {
    XDrawRectangle(dpy, c->scr->back, c->scr->rubbergc,
                   x, y, c->pwidth-1, c->pheight-1);
  }
  for (;;) {
    XEvent event;
    event = get_drag_event();
    if(!prefs.opaquemove) {
      XDrawRectangle(dpy, c->scr->back, c->scr->rubbergc,
                     x, y, c->pwidth-1, c->pheight-1);
    }
    if (event.type == ButtonRelease && event.xbutton.button == Button1) {
      XMoveWindow(dpy, c->parent, c->x = x, c->y = y);
      break;
    }
    if (event.type == ButtonPress && event.xbutton.button == Button3)
      break;
    if (event.type != MotionNotify)
      continue;

    x = event.xmotion.x_root - dx;
    y = event.xmotion.y_root - dy;
    if(!forcemoving) {
      if (prefs.forcemove==FM_AUTO &&
         (x + c->pwidth  - c->scr->width  > (c->pwidth>>2) ||
          y + c->pheight - c->scr->height > (c->pheight>>2) ||
          -x > (c->pwidth>>2) || -y > (c->pheight>>2)))
        forcemoving=1;
      else {
        if (x + c->pwidth  > c->scr->width)
          x = c->scr->width-c->pwidth;
        if (y + c->pheight > c->scr->height)
          y = c->scr->height-c->pheight;
        if (x < 0)
          x = 0;
        if (y < 0)
          y = 0;
      }
    }
    if(prefs.opaquemove) {
      if (y <= -c->scr->bh)
        y = 1 - c->scr->bh;
      XMoveWindow(dpy, c->parent, c->x = x, c->y = y);
    } else {
      XDrawRectangle(dpy, c->scr->back, c->scr->rubbergc,
                     x, y, c->pwidth-1, c->pheight-1);
    }
  }
  sendconfig(c);
  ungrab_server();
  XUngrabPointer(dpy, CurrentTime);
}

static void drag_screen(Scrn *s, Time time, int x_root, int y_root)
{
  int y = s->y;
  int dy = y_root - s->y;

  if (!grab_for_motion(s->menubar, None, None, time, x_root, y_root))
    return;
  for (;;) {
    XEvent event;
    event = get_drag_event();
    if (event.type == ButtonRelease && event.xbutton.button == Button1) {
      s->y = y;
      break;
    }
    if (event.type == ButtonPress && event.xbutton.button == Button3)
      break;
    if (event.type != MotionNotify)
      continue;

    y = event.xmotion.y_root - dy;
#warning TODO: when add multihead, drag screen relative to monitor geometry
    if (y < 0)
      y = 0;
    if (y >= s->height)
      y = s->height - 1;
    XMoveWindow(dpy, s->back, -s->bw, y - s->bw);
  }
  XMoveWindow(dpy, s->back, -s->bw, s->y - s->bw);
  XUngrabPointer(dpy, CurrentTime);
#ifndef ASSIMILATE_WINDOWS
  scrsendconfig(s);
#endif
}

static void drag_icon(Scrn *s, Time time, int x_root, int y_root)
{
  int nicons = 0;
  struct {
    Icon *icon;
    Window w;
    Pixmap pm;
    int x, y;
  } *dragging = NULL;
  Icon *i;

  for (i = s->firstselected; i != NULL; i = i->nextselected)
    nicons++;
  if (nicons < 1)
    return;
  dragging = calloc(nicons, sizeof(*dragging));
  if (dragging == NULL) {
    /* is XBell(3) the best way to signal error? */
    XBell(dpy, 100);
    return;
  }
  if (!grab_for_motion(s->back, None, None, time, x_root, y_root)) {
    free(dragging);
    return;
  }
  for (nicons = 0, i = s->firstselected; i != NULL; i = i->nextselected, nicons++) {
    XWindowAttributes xwa;
    XSetWindowAttributes xswa;

    dragging[nicons].icon = i;
    XGetWindowAttributes(dpy, i->window, &xwa);
    XTranslateCoordinates(dpy, i->window, s->root, 0, 0,
                          &dragging[nicons].x, &dragging[nicons].y,
                          &(Window){0});
    dragging[nicons].x += 4;
    dragging[nicons].y += 4;
    xswa.save_under = True;
    xswa.override_redirect = True;
    if (i->innerwin != None) {
      XGetWindowAttributes(dpy, i->innerwin, &xwa);
      xswa.background_pixmap = dragging[nicons].pm = XCreatePixmap(
        dpy, i->innerwin, xwa.width, xwa.height, xwa.depth
      );
      XCopyArea(dpy, i->innerwin, dragging[nicons].pm, s->gc,
                0, 0, xwa.width, xwa.height, 0, 0);
    } else {
      if (i->secondpm) {
        xswa.background_pixmap = i->secondpm;
        XGetGeometry(dpy, i->secondpm, &xwa.root, &(int){0}, &(int){0},
                     (unsigned int *)&xwa.width, (unsigned int *)&xwa.height,
                     &(unsigned int){0}, (unsigned int *)&xwa.depth);
      } else if (i->iconpm) {
        xswa.background_pixmap = i->iconpm;
        XGetGeometry(dpy, i->iconpm, &xwa.root, &(int){0}, &(int){0},
                     (unsigned int *)&xwa.width, (unsigned int *)&xwa.height,
                     &(unsigned int){0}, (unsigned int *)&xwa.depth);
      }
      if (xwa.depth != s->depth) {
        dragging[nicons].pm = XCreatePixmap(dpy, i->window, xwa.width,
                                            xwa.height, xwa.depth);
        XSetForeground(dpy, s->gc, s->dri.dri_Pens[SHADOWPEN]);
        XSetBackground(dpy, s->gc, s->dri.dri_Pens[BACKGROUNDPEN]);
        XCopyPlane(dpy, xswa.background_pixmap, dragging[nicons].pm,
                   s->gc, 0, 0, xwa.width, xwa.height, 0, 0, 1);
        xswa.background_pixmap = dragging[nicons].pm;
        xwa.depth = s->depth;
      }
    }
    xswa.colormap = xwa.colormap;
    dragging[nicons].w = XCreateWindow(
      dpy, s->root, dragging[nicons].x, dragging[nicons].y,
      xwa.width, xwa.height, 0, xwa.depth, xwa.class, xwa.visual,
      CWBackPixmap|CWOverrideRedirect|CWSaveUnder|CWColormap, &xswa
    );
#ifdef HAVE_XSHAPE
    if (shape_extn) {
      if (i->innerwin != None) {
        int b_shaped;
        XShapeQueryExtents(
          dpy, i->innerwin,
          &b_shaped, &(int){0}, &(int){0}, &(unsigned){0}, &(unsigned){0},
          &(int){0}, &(int){0}, &(int){0}, &(unsigned){0}, &(unsigned){0}
        );
        if (b_shaped) XShapeCombineShape(
          dpy, dragging[nicons].w, ShapeBounding, 0, 0, i->innerwin,
          ShapeBounding, ShapeSet
        );
      } else if (i->maskpm != None) {
        XShapeCombineMask(
          dpy, dragging[nicons].w, ShapeBounding, 0, 0, i->maskpm, ShapeSet
        );
      }
    }
#endif
    XMapRaised(dpy, dragging[nicons].w);
  }

  for (;;) {
    XEvent event;
    event = get_drag_event();
    if (event.type == ButtonRelease && event.xbutton.button == Button1) {
      Scrn *s = get_front_scr();
      int wx, wy;
      Window ch;
      Client *c;

      for (;;) {
        if (s->root == event.xbutton.root && event.xbutton.y_root >= s->y)
          break;
        s = s->behind;
        if (s == get_front_scr()) {
          goto error;
        }
      }
      if (s->deftitle == NULL)
        goto error;
      if (XTranslateCoordinates(dpy, s->root, s->back, x_root, y_root,
                                &wx, &wy, &ch) && ch != None) {
        if (XFindContext(dpy, ch, client_context, (void *)&c) ||
            c->scr != s || c->state != NormalState) {
          c = NULL;
        }
      } else {
        c = NULL;
      }

      if (c != NULL) {
        if (c->module != NULL) {
          XTranslateCoordinates(dpy, s->back, c->window, -4, -4, &wx, &wy, &ch);
          for (int n = 0; n < nicons; n++) {
            dispatch_event_to_broker(mkcmessage(
              c->window, ATOMS[AMIWM_APPWINDOWMSG],dragging[n].icon->window,
              dragging[n].x + wx, dragging[n].y + wy
            ), 0, c->module);
          }
          goto done;
        } else {
          goto error;
        }
      }

      for (int n = 0; n < nicons; n++) {
        if (dragging[n].icon->scr != s) {
          dragging[n].icon->mapped = False; /* reparenticon() needs it false */
          reparenticon(dragging[n].icon, s,
                       dragging[n].x - 4, dragging[n].y - 4 - s->y);
        } else {
          XMoveWindow(dpy, dragging[n].icon->window,
                      dragging[n].icon->x = dragging[n].x - 4,
                      dragging[n].icon->y = dragging[n].y - 4 - s->y);
        }
        dragging[n].icon->mapped = True;
        adjusticon(dragging[n].icon);
      }

      goto done;
    }
    if (event.type == ButtonPress && event.xbutton.button == Button3)
      break;
    if (event.type != MotionNotify)
      continue;

    for (int n = 0; n < nicons; n++) {
      int dx = x_root - dragging[n].x;
      int dy = y_root - dragging[n].y;
      XMoveWindow(dpy, dragging[n].w,
                  dragging[n].x = event.xmotion.x_root - dx,
                  dragging[n].y = event.xmotion.y_root - dy);
    }
    x_root = event.xmotion.x_root;
    y_root = event.xmotion.y_root;
  }
error:
  wberror(dragging[0].icon->scr,
          "Icon can't be moved into this window");

done:
  for (int n = 0; n < nicons; n++) {
    XDestroyWindow(dpy, dragging[n].w);
    if(dragging[n].pm)
      XFreePixmap(dpy, dragging[n].pm);
  }
  free(dragging);
  XUngrabPointer(dpy, CurrentTime);
}

static void drag_resize(Client *c, Time time, int x_root, int y_root)
{
  int w = c->pwidth;
  int h = c->pheight;
  int dx = w - (x_root - c->x);
  int dy = h - (y_root - c->y);

  if (!grab_for_motion(c->drag, c->scr->back, None, time, x_root, y_root))
    return;
  grab_server();
  if(!prefs.opaqueresize)
    XDrawRectangle(dpy, c->scr->back, c->scr->rubbergc, c->x, c->y, w-1, h-1);
  for (;;) {
    XEvent event;
    event = get_drag_event();
    if(!prefs.opaqueresize)
      XDrawRectangle(dpy, c->scr->back, c->scr->rubbergc, c->x, c->y, w-1, h-1);
    if (event.type == ButtonRelease && event.xbutton.button == Button1) {
      resizeclientwindow(c, w, h);
      break;
    }
    if (event.type == ButtonPress && event.xbutton.button == Button3)
      break;
    if (event.type != MotionNotify)
      continue;

    w = event.xmotion.x_root - c->x + dx;
    h = event.xmotion.y_root - c->y + dy;
    if (c->sizehints->width_inc) {
      w -= c->sizehints->base_width + c->framewidth;
      w -= w % c->sizehints->width_inc;
      w += c->sizehints->base_width;
      if (w > c->sizehints->max_width)
        w = c->sizehints->max_width;
      if (w < c->sizehints->min_width)
        w = c->sizehints->min_width;
      w += c->framewidth;
    }
    if (c->sizehints->height_inc) {
      h -= c->sizehints->base_height + c->frameheight;
      h -= h % c->sizehints->height_inc;
      h += c->sizehints->base_height;
      if (h > c->sizehints->max_height)
        h = c->sizehints->max_height;
      if (h < c->sizehints->min_height)
        h = c->sizehints->min_height;
      h += c->frameheight;
    }
    if(prefs.opaqueresize) {
      resizeclientwindow(c, w, h);
    } else {
      XDrawRectangle(dpy, c->scr->back, c->scr->rubbergc, c->x, c->y, w-1, h-1);
    }
  }
  sendconfig(c);
  ungrab_server();
  XUngrabPointer(dpy, CurrentTime);
}

static void do_icon_double_click(Scrn *scr)
{
  Icon *i, *next;

  for(i=scr->firstselected; i; i=next) {
    next=i->nextselected;
    if(i->module) {
      dispatch_event_to_broker(mkcmessage(i->window, ATOMS[AMIWM_APPICONMSG], 0),
			       0, i->module);
    } else {
      deiconify(i);
    }
  }
}

static void abortfocus()
{
  if(activeclient) {
    activeclient->active=False;
    redrawclient(activeclient);
    if(prefs.focus==FOC_CLICKTOTYPE)
      XGrabButton(dpy, Button1, AnyModifier, activeclient->parent,
		  True, ButtonPressMask, GrabModeSync, GrabModeAsync,
		  None, wm_curs);
    activeclient = NULL;
  }
  setfocus(None);
}

static RETSIGTYPE sighandler(int sig)
{
  signalled=1;
  signal(sig, SIG_IGN);
}

static void instcmap(Colormap c)
{
  XInstallColormap(dpy, (c == None) ? scr->cmap : c);
}

static void update_clock(void *dontcare)
{
  Scrn *scr;

  if(server_grabs)
    return;
  call_out(prefs.titleclockinterval, 0, update_clock, dontcare);

  scr = get_front_scr();
  do {
    redrawmenubar(scr, scr->menubar);
    scr=scr->behind;
  } while(scr != get_front_scr());
}

static void cleanup()
{
  int sc;
  extern void free_prefs();
  struct coevent *e;
  flushmodules();
  flushclients();
  scr = get_front_scr();
  for(sc=0; checkwins!=NULL && sc<ScreenCount(dpy); sc++)
    XDestroyWindow(dpy, checkwins[sc]);
  free(checkwins);
  while(scr)
    closescreen();
  free_prefs();
  if(dpy) {
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    XFlush(dpy);
    XCloseDisplay(dpy);
  }
  while((e = eventlist)) {
    eventlist = e->next;
    free(e);
  }
  if(x_server)
    free(x_server);
}

int main(int argc, char *argv[])
{
  int x_fd, sc;
  static Argtype array[3];
  struct RDArgs *ra;

#ifdef USE_FONTSETS
  setlocale(LC_CTYPE, "");
  setlocale(LC_TIME, "");
#endif

  main_argv=argv;
  progname=argv[0];

  atexit(cleanup);

  memset(array, 0, sizeof(array));
  initargs(argc, argv);
  if(!(ra=ReadArgs((UBYTE *)"RCFILE,DISPLAY/K,SINGLE/S",
		   (LONG *)array, NULL))) {
    PrintFault(IoErr(), (UBYTE *)progname);
    exit(1);
  }

  x_server = strdup(XDisplayName(array[1].ptr));

  XrmInitialize();

  if(!(dpy = XOpenDisplay(array[1].ptr))) {
    fprintf(stderr, "%s: cannot connect to X server %s\n", progname, x_server);
    FreeArgs(ra);
    exit(1);
  }

  if(array[1].ptr) {
    char *env=malloc(strlen((char *)array[1].ptr)+10);
    sprintf(env, "DISPLAY=%s", (char *)array[1].ptr);
    putenv(env);
  }

  client_context = XUniqueContext();
  screen_context = XUniqueContext();
  icon_context = XUniqueContext();
  menu_context = XUniqueContext();
  vroot_context = XUniqueContext();

  wm_curs=XCreateFontCursor(dpy, XC_top_left_arrow);

  FD_ZERO(&master_fd_set);
  x_fd=ConnectionNumber(dpy);
  FD_SET(x_fd, &master_fd_set);
  max_fd=x_fd+1;

  initting = 1;
  XSetErrorHandler(handler);

#ifdef HAVE_XSHAPE
  if(XShapeQueryExtension(dpy, &shape_event_base, &shape_error_base))
    shape_extn = 1;
#endif

  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSelectInput(dpy, DefaultRootWindow(dpy), NoEventMask);

  init_modules();
  read_rc_file(array[0].ptr, !array[2].num);
  if( prefs.titleclockinterval < 1 ) prefs.titleclockinterval = 1;
  FreeArgs(ra);

  if (signal(SIGTERM, sighandler) == SIG_IGN)
    signal(SIGTERM, SIG_IGN);
  if (signal(SIGINT, sighandler) == SIG_IGN)
    signal(SIGINT, SIG_IGN);
#ifdef SIGHUP
  if (signal(SIGHUP, sighandler) == SIG_IGN)
    signal(SIGHUP, SIG_IGN);
#endif

  init_atoms();

#ifndef AMIGAOS
  if((fcntl(ConnectionNumber(dpy), F_SETFD, 1)) == -1)
    fprintf(stderr, "%s: child cannot disinherit TCP fd\n", progname);
#endif

  checkwins = calloc(ScreenCount(dpy), sizeof(*checkwins));
  for(sc=0; sc<ScreenCount(dpy); sc++) {
    if(sc==DefaultScreen(dpy) || prefs.manage_all) {
      Window root = RootWindow(dpy, sc);
      checkwins[sc] = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 1, 1, 1);
      setsupports(scr->root, checkwins[sc]);
      if(!getscreenbyroot(root)) {
	char buf[64];
	sprintf(buf, "Screen.%d", sc);
	openscreen((sc? strdup(buf):"Workbench Screen"), root);
      }
    }
  }
  /*
  if(!front)
    openscreen("Workbench Screen", DefaultRootWindow(dpy));
    */
  realizescreens();

  setfocus(None);

  initting = 0;

  if(prefs.titlebarclock)
    call_out(0, 0, update_clock, NULL);

  while(!signalled) {
    fd_set rfds;
    struct timeval t;
    XEvent event;
    Window dummy_root;
    int dummy_x, dummy_y;
    unsigned int dummy_w, dummy_h, dummy_bw, dummy_d;

    while((!signalled) && QLength(dpy)>0) {
      Client *c; Icon *i;

      event = get_next_event();
      if(!XFindContext(dpy, event.xany.window, client_context,
		       (XPointer*)&c)) {
	scr=c->scr;
      } else {
	c = NULL;
	if(XFindContext(dpy, event.xany.window, screen_context,
			(XPointer*)&scr))
	  scr = get_front_scr();
      }
      if(XFindContext(dpy, event.xany.window, icon_context, (XPointer*)&i))
	i=NULL;
      else
	scr=i->scr;
      switch(event.type) {
      case CreateNotify:

	if(!XFindContext(dpy, event.xcreatewindow.window, client_context,
			 (XPointer *)&c)) {
	  break;
	}

	if(!event.xcreatewindow.override_redirect) {
	  if(!(scr=getscreenbyroot(event.xcreatewindow.parent)))
	    scr = get_front_scr();
	  createclient(event.xcreatewindow.window);
	}
#ifdef ASSIMILATE_WINDOWS
	else if(XFindContext(dpy, event.xcreatewindow.window, screen_context, (XPointer*)&scr)
		&& (scr=getscreenbyroot(event.xcreatewindow.parent))) {
	  XGetWindowAttributes(dpy, event.xcreatewindow.window, &attr);
	  assimilate(event.xcreatewindow.window, attr.x, attr.y);
	}
#endif
	break;
      case DestroyNotify:
	if(!XFindContext(dpy, event.xdestroywindow.window, client_context,
			 (XPointer*)&c)) {
	  ignore_badwindow = 1;
	  rmclient(c);
	  XSync(dpy, False);
	  ignore_badwindow = 0;
	} else if(!XFindContext(dpy, event.xdestroywindow.window, icon_context,
				(XPointer*)&i)) {
	  ignore_badwindow = 1;
	  if(i->client)
	    i->client->icon=NULL;
	  rmicon(i);
	  XSync(dpy, False);
	  ignore_badwindow = 0;	  
	} else if(event.xdestroywindow.window)
	  XDeleteContext(dpy, event.xdestroywindow.window, screen_context);
	break;
      case UnmapNotify:
	if(c && c->active && (event.xunmap.window==c->parent) &&
	   !(c->fullscreen && c->state == NormalState)) {
	  c->active=False;
	  activeclient = NULL;
	  redrawclient(c);
	  if(prefs.focus == FOC_CLICKTOTYPE)
	    XGrabButton(dpy, Button1, AnyModifier, c->parent, True,
			ButtonPressMask, GrabModeSync, GrabModeAsync,
			None, wm_curs);
	}
	if(c && (event.xunmap.window==c->window)) {
	  if((!c->reparenting) && c->parent != c->scr->root) {
	    Icon *i=c->icon;
	    XUnmapWindow(dpy, c->parent);
	    if(i) {
	      if(i->labelwin)
		XUnmapWindow(dpy, i->labelwin);
	      if(i->window)
		XUnmapWindow(dpy, i->window);
	      i->mapped=0;
	      deselecticon(i);
	    }
	    setclientstate(c, WithdrawnState);
	  }
	  c->reparenting = 0;
	}
	break;
      case ConfigureNotify:
	if((!XFindContext(dpy, event.xconfigure.window, icon_context,
			 (XPointer *)&i)) &&
	   event.xconfigure.window == i->window){
	  i->x=event.xconfigure.x; i->y=event.xconfigure.y;
	  i->width=event.xconfigure.width; i->height=event.xconfigure.height;
	  if(i->labelwin) {
	    XWindowChanges xwc;
	    xwc.x=i->x+(i->width>>1)-(i->labelwidth>>1);
	    xwc.y=i->y+i->height+1;
	    xwc.sibling=i->window;
	    xwc.stack_mode=Below;
	    XConfigureWindow(dpy, i->labelwin, CWX|CWY|CWSibling|CWStackMode,
			       &xwc);
	  }
	}
	break;
      case ReparentNotify:
	if((!XFindContext(dpy, event.xreparent.window, icon_context,
			 (XPointer *)&i)) &&
	   event.xreparent.window == i->window){
	  i->parent=event.xreparent.parent;
	  i->x=event.xreparent.x; i->y=event.xreparent.y;
	  if(i->labelwin) {
	    XWindowChanges xwc;
	    XReparentWindow(dpy, i->labelwin, i->parent,
			i->x+(i->width>>1)-(i->labelwidth>>1),
			i->y+i->height+1);
	    xwc.sibling=i->window;
	    xwc.stack_mode=Below;
	    XConfigureWindow(dpy, i->labelwin, CWSibling|CWStackMode, &xwc);
	  }
	}
	break;
      case ClientMessage:
	if(c)
	  handle_client_message(c, &event.xclient);
	break;
      case ColormapNotify:
	if(event.xcolormap.new && c)
	  if(c->colormap!=event.xcolormap.colormap) {
	    c->colormap=event.xcolormap.colormap;
	    if(c->active)
	      instcmap(c->colormap);
	  }
	break;
      case ConfigureRequest:
	if(XFindContext(dpy, event.xconfigurerequest.window, client_context,
			(XPointer*)&c))
	  c = NULL;
	if(c && event.xconfigurerequest.window==c->window &&
	   c->parent!=c->scr->root) {
	  extern void resizeclientwindow(Client *c, int, int);
	  if(event.xconfigurerequest.value_mask&CWBorderWidth)
	    c->old_bw=event.xconfigurerequest.border_width;
	  resizeclientwindow(c, (event.xconfigurerequest.value_mask&CWWidth)?
			     event.xconfigurerequest.width+c->framewidth:c->pwidth,
			     (event.xconfigurerequest.value_mask&CWHeight)?
			     event.xconfigurerequest.height+c->frameheight:c->pheight);
	  if((event.xconfigurerequest.value_mask&(CWX|CWY)) &&
	     c->state==WithdrawnState)
	    XMoveWindow(dpy, c->parent,
			c->x=((event.xconfigurerequest.value_mask&CWX)?
			      event.xconfigurerequest.x:c->x),
			c->y=((event.xconfigurerequest.value_mask&CWY)?
			      event.xconfigurerequest.y:c->y));
	} else {
	  if(!XFindContext(dpy, event.xconfigurerequest.window,
			    screen_context, (XPointer *)&scr))
	    if((event.xconfigurerequest.y-=scr->y)<0)
	      event.xconfigurerequest.y=0;
	  XConfigureWindow(dpy, event.xconfigurerequest.window,
			   event.xconfigurerequest.value_mask,
			   (XWindowChanges *)&event.xconfigurerequest.x);
	}
	break;
      case CirculateRequest:
	if(XFindContext(dpy, event.xcirculaterequest.window, client_context, (XPointer*)&c))
	  if(event.xcirculaterequest.place==PlaceOnTop)
	    XRaiseWindow(dpy, event.xcirculaterequest.window);
	  else {
	    Client *c2;
	    Window r,p,*children;
	    unsigned int nchildren;
	    if(XQueryTree(dpy, scr->back, &r, &p, &children, &nchildren)) {
	      int n;
	      for(n=0; n<nchildren; n++)
		if((!XFindContext(dpy, children[n], client_context, (XPointer*)&c2)) &&
		   children[n]==c2->parent)
		  break;
	      if(n<nchildren) {
		Window ws[2];
		ws[0]=children[n];
		ws[1]=event.xcirculaterequest.window;
		XRestackWindows(dpy, ws, 2);
	      }
	    }
	  }
	else
	  raiselowerclient(c, event.xcirculaterequest.place);
	break;
      case MapRequest:
	if(XFindContext(dpy, event.xmaprequest.window, client_context, (XPointer*)&c)) {
	  if(!(scr=getscreenbyroot(event.xmaprequest.parent)))
	    scr = get_front_scr();
	  c=createclient(event.xmaprequest.window);
	}
	{
	  XWMHints *xwmh;
	  if(c->parent==c->scr->root && (xwmh=XGetWMHints(dpy, c->window))) {
	    if(c->state==WithdrawnState && (xwmh->flags&StateHint)
	       && xwmh->initial_state==IconicState)
	      c->state=IconicState;
	    XFree(xwmh);
	  }
	  switch(c->state) {
	  case WithdrawnState:
	    if(c->parent == c->scr->root)
	      reparent(c);
	  case NormalState:
	    XMapWindow(dpy, c->window);
	    if (!c->fullscreen)
	      XMapRaised(dpy, c->parent);
	    setclientstate(c, NormalState);
	    break;
	  case IconicState:
	    if(c->parent == c->scr->root)
	      reparent(c);
            iconify(c);
	    break;
	  }
	}
	break;
      case MapNotify:
	if(prefs.focus == FOC_CLICKTOTYPE && c &&
	   event.xmap.window == c->parent && c->parent != c->scr->root &&
	   (!c->active)) {
	  if(activeclient) {
	    XGrabButton(dpy, Button1, AnyModifier, activeclient->parent,
			True, ButtonPressMask, GrabModeSync, GrabModeAsync,
			None, wm_curs);
	    activeclient->active=False;
	    redrawclient(activeclient);
	  }
	  c->active=True;
	  activeclient = c;
	  XUngrabButton(dpy, Button1, AnyModifier, c->parent);
	  redrawclient(c);
	  setfocus(c->window);
	}
	break;
      case EnterNotify:
	if(c) {
	  if((!c->active) && (c->state==NormalState) &&
	     prefs.focus!=FOC_CLICKTOTYPE) {
	    if(activeclient) {
	      activeclient->active=False;
	      redrawclient(activeclient);
	    }
	    setfocus(c->window);
	    c->active=True;
	    activeclient = c;
	    redrawclient(c);
	    if(prefs.autoraise && c->parent!=c->scr->root)
	      XRaiseWindow(dpy, c->parent);
	  }
	  if(event.xcrossing.window==c->window)
	    instcmap(c->colormap);
	}
	break;
      case LeaveNotify:
	if(c) {
	  if(c->active && event.xcrossing.window==c->parent &&
	     event.xcrossing.detail!=NotifyInferior &&
	     prefs.focus == FOC_FOLLOWMOUSE) {
	    c->active=False;
	    activeclient = NULL;
	    instcmap(None);
	    redrawclient(c);
	  } else if(event.xcrossing.window==c->window &&
		    event.xcrossing.detail!=NotifyInferior &&
		    event.xcrossing.mode==NotifyNormal)
	    instcmap(None);
	}
	break;
      case ButtonPress:
	if(event.xbutton.button==Button1) {
	  if(c) {
	    if((!c->active) && prefs.focus==FOC_CLICKTOTYPE &&
	       (c->state==NormalState)) {
	      if(activeclient) {
		activeclient->active=False;
		redrawclient(activeclient);
		XGrabButton(dpy, Button1, AnyModifier, activeclient->parent,
			    True, ButtonPressMask, GrabModeSync, GrabModeAsync,
			    None, wm_curs);
	      }
	      setfocus(c->window);
	      c->active=True;
	      activeclient = c;
	      redrawclient(c);
	      XUngrabButton(dpy, Button1, AnyModifier, c->parent);
	      if(prefs.autoraise && c->parent!=c->scr->root)
		XRaiseWindow(dpy, c->parent);
	    }
	    if(event.xbutton.window!=c->depth &&
	       event.xbutton.window!=c->window) {
	      if(c==doubleclient && (event.xbutton.time-last_double)<
		 dblClickTime) {
		XRaiseWindow(dpy, c->parent);
	      } else {
		doubleclient=c;
		last_double=event.xbutton.time;
	      }
	    }
	    if(event.xbutton.window==c->drag) {
	      forcemoving=(prefs.forcemove==FM_ALWAYS) ||
		(event.xbutton.state & ShiftMask);
	      drag_move(c, event.xbutton.time, event.xbutton.x_root, event.xbutton.y_root);
	    } else if(event.xbutton.window==c->resize)
	      drag_resize(c, event.xbutton.time, event.xbutton.x_root, event.xbutton.y_root);
	    else if(event.xbutton.window==c->window ||
		    event.xbutton.window==c->parent)
	      ;
	    else if (event.xbutton.window == c->close) {
              click_close(c, event.xbutton.time);
	    } else if (event.xbutton.window == c->iconify) {
              click_iconify(c, event.xbutton.time);
	    } else if (event.xbutton.window == c->depth) {
              click_depth(c, event.xbutton.time);
	    } else if (event.xbutton.window == c->zoom) {
              click_zoom(c, event.xbutton.time);
            }
	  } else if(i && event.xbutton.window==i->window) {
	    abortfocus();
	    if(i->selected && (event.xbutton.time-last_icon_click)<dblClickTime) {
	      do_icon_double_click(i->scr);
	    } else {
	      if(!(event.xbutton.state & ShiftMask))
		deselect_all_icons(i->scr);
	      last_icon_click=event.xbutton.time;
	      selecticon(i);
              drag_icon(i->scr, event.xbutton.time, event.xbutton.x_root, event.xbutton.y_root);
	    }
	  } else if(scr&&event.xbutton.window==scr->menubardepth) {
	    click_screendepth(scr, event.xbutton.time);
	  } else if(scr&&event.xbutton.window==scr->menubar &&
		    scr->back!=scr->root) {
	    drag_screen(scr, event.xbutton.time, event.xbutton.x_root, event.xbutton.y_root);
	  } else if(scr&&scr->back==event.xbutton.window) {
	    abortfocus();
	    drag_bounding(scr, event.xbutton.time,
                          event.xbutton.x_root, event.xbutton.y_root,
                          event.xbutton.x, event.xbutton.y);
	  } else ;
	} else if(event.xbutton.button==3) {
	  if(c == NULL && scr != NULL) {
	    drag_menu(scr);
	  }
	}
	if(prefs.focus == FOC_CLICKTOTYPE) {
	  XSync(dpy,0);
	  XAllowEvents(dpy,ReplayPointer,CurrentTime);
	  XSync(dpy,0);
	}
	break;
      case PropertyNotify:
	if(event.xproperty.atom != None && c &&
	   event.xproperty.window==c->window &&
	   XGetGeometry(dpy, c->window, &dummy_root, &dummy_x, &dummy_y,
			&dummy_w, &dummy_h, &dummy_bw, &dummy_d))
	  propertychange(c, event.xproperty.atom);
	break;
      case FocusOut:
	/* Ignore */
	break;
      case FocusIn:
	if(event.xfocus.detail == NotifyDetailNone &&
	   prefs.focus == FOC_CLICKTOTYPE &&
	   (scr = getscreenbyroot(event.xfocus.window))) {
	  Window w;
	  int rt;
	  XGetInputFocus(dpy, &w, &rt);
	  if(w == None)
	    setfocus(scr->inputbox);
	}
	break;
      default:
#ifdef HAVE_XSHAPE
	if(shape_extn && event.type == shape_event_base + ShapeNotify) {
	  XShapeEvent *s = (XShapeEvent *) &event;
	  if(c && s->kind == ShapeBounding) {
	    c->shaped = s->shaped;
	    reshape_frame(c);
	  }
	  break;
	}
#endif
	fprintf(stderr, "%s: got unexpected event type %d.\n",
		progname, event.type);
      }
    }
    if(signalled) break;
    rfds = master_fd_set;
    t.tv_sec = t.tv_usec = 0;
    if (select(max_fd, &rfds, NULL, NULL, &t) > 0) {
      handle_module_input(&rfds);
      if(FD_ISSET(x_fd, &rfds))
	XPeekEvent(dpy, &event);
      continue;
    }
    if(signalled) break;
    XFlush(dpy);
    rfds = master_fd_set;
    if(eventlist)
      fill_in_call_out(&t);
    if(select(max_fd, &rfds, NULL, NULL, (eventlist? &t:NULL))<0) {
      if (errno != EINTR) {
	perror("select");
	break;
      }
    } else {
      call_call_out();
      handle_module_input(&rfds);
      if(FD_ISSET(x_fd, &rfds))
	XPeekEvent(dpy, &event);
    }
  }

  if(prefs.titlebarclock)
    remove_call_out(update_clock, NULL);

  if(signalled)
    fprintf(stderr, "%s: exiting on signal\n", progname);

  exit(signalled? 0:1);
}

