#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#ifdef AMIGAOS
#include <x11/xtrans.h>
#endif

#include "client.h"
#include "events.h"
#include "frame.h"
#include "icon.h"
#include "menu.h"
#include "module.h"
#include "screen.h"

#ifdef BSD_STYLE_GETTIMEOFDAY
#define GETTIMEOFDAY(tp) gettimeofday(tp, NULL)
#else
#define GETTIMEOFDAY(tp) gettimeofday(tp)
#endif
#define FIXUPTV(tv) { \
    while((tv).tv_usec<0) { (tv).tv_usec+=1000000; (tv).tv_sec--; } \
    while((tv).tv_usec>=1000000) { (tv).tv_usec-=1000000; (tv).tv_sec++; } \
}

struct coevent *eventlist=NULL;

fd_set master_fd_set;
int max_fd=0;

extern Display *dpy;
extern XContext client_context, icon_context, screen_context;

static void restorescreentitle(Scrn *s)
{
  (scr=s)->title=s->deftitle;
  XClearWindow(dpy, s->menubar);
  redrawmenubar(s, s->menubar);
  if(free_screentitle) {
    free(free_screentitle);
    free_screentitle=NULL;
  }
}

static void handle_other_events(void)
{
  fd_set rfds = master_fd_set;
  struct timeval t = {
    .tv_sec = 0,
    .tv_usec = 0,
  };

  if (select(max_fd, &rfds, NULL, NULL, &t) > 0) {
    handle_module_input(&rfds);
    if(FD_ISSET(ConnectionNumber(dpy), &rfds))
      XFlush(dpy);
    return;
  }
  XFlush(dpy);
  rfds = master_fd_set;
  if (eventlist)
    fill_in_call_out(&t);
  if (select(max_fd, &rfds, NULL, NULL, (eventlist? &t:NULL))<0) {
    if (errno != EINTR) {
      perror("select");
      return;
    }
  } else {
    call_call_out();
    handle_module_input(&rfds);
    if(FD_ISSET(ConnectionNumber(dpy), &rfds))
      XFlush(dpy);
  }
}

static Bool filter_event(XEvent *event)
{
  Client *c = NULL;
  Icon *i = NULL;
  Scrn *s = NULL;

  switch (event->type) {
  case Expose:
    if (event->xexpose.count)
      return True;
    else if (!XFindContext(dpy, event->xexpose.window, client_context, (XPointer*)&c))
      redraw(c, event->xexpose.window);
    else if (!XFindContext(dpy, event->xexpose.window, icon_context, (XPointer*)&i))
      redrawicon(i, event->xexpose.window);
    else if (!XFindContext(dpy, event->xexpose.window, screen_context, (XPointer*)&s))
      redrawmenubar(s, event->xexpose.window);
    return True;
  case KeyPress:
    if(!dispatch_event_to_broker(event, KeyPressMask, modules))
      internal_broker(event);
    return True;
  case KeyRelease:
    if(!dispatch_event_to_broker(event, KeyPressMask, modules))
      internal_broker(event);
    return True;
  case MappingNotify:
    if(!dispatch_event_to_broker(event, 0, modules))
      internal_broker(event);
    return True;
  case MotionNotify:
    compress_motion(event);
    return False;  /* motion events are filtered; but caller must still handle it */
  default:
    return False;  /* unfiltered event; caller must handle it */
  }
}

void add_fd_to_set(int fd)
{
  FD_SET(fd, &master_fd_set);
  if(fd>=max_fd)
    max_fd=fd+1;
}

void remove_fd_from_set(int fd)
{
  FD_CLR(fd, &master_fd_set);
}

void call_out(int howlong_s, int howlong_u, void (*what)(void *), void *with)
{
  struct coevent *ce=malloc(sizeof(struct coevent));
  if(ce) {
    struct coevent **e=&eventlist;
    GETTIMEOFDAY(&ce->when);
    ce->when.tv_sec+=howlong_s;
    ce->when.tv_usec+=howlong_u;
    FIXUPTV(ce->when);
    ce->what=what;
    ce->with=with;
    while(*e && ((*e)->when.tv_sec<ce->when.tv_sec ||
		 ((*e)->when.tv_sec==ce->when.tv_sec &&
		  (*e)->when.tv_usec<=ce->when.tv_usec)))
      e=&(*e)->next;
    ce->next=*e;
    *e=ce;
  }
}

void remove_call_out(void (*what)(void *), void *with)
{
  struct coevent *ee, **e=&eventlist;

  while(*e && ((*e)->what != what || (*e)->with != with))
    e=&(*e)->next;
  if((ee=*e)) {
    *e=(*e)->next;
    free(ee);
  }
}

void fill_in_call_out(struct timeval *tv)
{
  GETTIMEOFDAY(tv);
  tv->tv_sec=eventlist->when.tv_sec-tv->tv_sec;
  tv->tv_usec=eventlist->when.tv_usec-tv->tv_usec;
  FIXUPTV(*tv);
  if(tv->tv_sec<0)
    tv->tv_sec = tv->tv_usec = 0;
}

void call_call_out(void)
{
  struct timeval now;
  struct coevent *e;
  GETTIMEOFDAY(&now);
  FIXUPTV(now);
  while((e=eventlist) && (e->when.tv_sec<now.tv_sec ||
			  (e->when.tv_sec==now.tv_sec &&
			   e->when.tv_usec<=now.tv_usec))) {
    eventlist=e->next;
    (e->what)(e->with);
    free(e);
  }
}

void wberror(Scrn *s, char *message)
{
  remove_call_out((void(*)(void *))restorescreentitle, s);
  (scr=s)->title=message;
  XClearWindow(dpy, s->menubar);
  redrawmenubar(s, s->menubar);
  XBell(dpy, 100);
  call_out(2, 0, (void(*)(void *))restorescreentitle, s);
}

void compress_motion(XEvent *event)
{
  if (event->type != MotionNotify)
    return;
  while (XCheckTypedWindowEvent(dpy, event->xmotion.window, MotionNotify, event))
    ;
}

Bool grab_for_motion(Window w, Window confine, Cursor curs, Time time, int x_root, int y_root)
{
  /*
   * To avoid misclicks, only grab for move/resize/etc, if the user has
   * moved the pointer by a given threshold.
   */
  static const int THRESHOLD = 5;
  int status;

  XSync(dpy, False);
  status = XGrabPointer(dpy, w, False,
                        ButtonPressMask|ButtonReleaseMask|Button1MotionMask,
                        GrabModeAsync, GrabModeAsync, confine, None, time);
  if (status == AlreadyGrabbed || status == GrabSuccess) {
    for (;;) {
      XEvent event;
      int dx, dy;

      XMaskEvent(dpy, ButtonPressMask|ButtonReleaseMask|Button1MotionMask, &event);
      XPutBackEvent(dpy, &event);
      if (event.type != MotionNotify)
        break;
      compress_motion(&event);
      dx = x_root - event.xmotion.x_root;
      dy = y_root - event.xmotion.y_root;
      if (dx*dx + dy*dy > THRESHOLD*THRESHOLD) /* Pythagoras! */
        return True;
    }
  }
  XUngrabPointer(dpy, CurrentTime);
  return False;
}

XEvent get_drag_event(void)
{
  for (;;) {
    XEvent event;
    while (XCheckMaskEvent(
      dpy, ExposureMask | ButtonPressMask | ButtonReleaseMask |
      PointerMotionMask | EnterWindowMask | LeaveWindowMask, &event
    )) {
      if (!filter_event(&event))
        return event;
    }
    handle_other_events();
  }
}

XEvent get_next_event(void)
{
  for (;;) {
    XEvent event;
    while (XPending(dpy) > 0) {
      XNextEvent(dpy, &event);
      if (!filter_event(&event))
        return event;
    }
    handle_other_events();
  }
}
