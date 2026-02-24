#ifndef ICC_H
#define ICC_H

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define Pdelete 1
#define Ptakefocus 2
#define Psizebottom 4
#define Psizeright 8
#define Psizetrans 16

#define mkcmessage(w, a, x, ...) (&(XEvent){.xclient = { \
  .type = ClientMessage, \
  .window = (w), \
  .message_type = (a), \
  .format = 32, \
  .data.l = {(long)(x), CurrentTime, __VA_ARGS__}, \
}})

#define NET_ATOMS \
  X(_NET_SUPPORTING_WM_CHECK) \
  X(_NET_WM_NAME) \
  X(_NET_WM_STATE) \
  X(_NET_WM_STATE_FULLSCREEN) \

#define ATOMS_TABLE(X) \
  X(__SWM_VROOT) \
  X(AMIWM_APPICONMSG) \
  X(AMIWM_APPWINDOWMSG) \
  X(AMIWM_SCREEN) \
  X(AMIWM_WFLAGS) \
  X(UTF8_STRING) \
  X(WM_CHANGE_STATE) \
  X(WM_CLASS) \
  X(WM_COLORMAP_WINDOWS) \
  X(WM_DELETE_WINDOW) \
  X(WM_PROTOCOLS) \
  X(WM_STATE) \
  X(WM_TAKE_FOCUS) \
  X(_NET_SUPPORTED) \
  NET_ATOMS     /* this must come last */

enum {
#define X(atom) atom,
  ATOMS_TABLE(X)
  NATOMS
#undef  X
};

extern Atom ATOMS[NATOMS];

extern Window get_transient_for(Window);
extern long _getprop(Window, Atom, Atom, long, char **);
extern void getproto(struct _Client *c);
extern void getwflags(struct _Client *);
extern void getwmstate(struct _Client *c);
extern void handle_client_message(struct _Client *, XClientMessageEvent *);
extern void init_atoms(void);
extern void propertychange(struct _Client *, Atom);
extern void sendcmessage(Window, Atom, long);
extern void setstringprop(Window, Atom, char *);
extern void setsupports(Window, Window);
extern void setwmstate(struct _Client *c);

#endif
