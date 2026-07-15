struct _Client;
struct _Scrn;

extern void click_close(struct _Client *c, Time time);
extern void click_depth(struct _Client *c, Time time);
extern void click_iconify(struct _Client *c, Time time);
extern void click_zoom(struct _Client *c, Time time);
extern void lowertopmostclient(struct _Scrn *scr);
extern void raisebottommostclient(struct _Scrn *scr);
extern void raiselowerclient(struct _Client *, int);
extern void redraw(struct _Client *, Window);
extern void redrawclient(struct _Client *);
extern void reparent(struct _Client *);
extern void reshape_frame(struct _Client *c);
extern void resizeclientwindow(struct _Client *c, int, int);
