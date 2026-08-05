struct _Client;
struct _Scrn;

extern void clickenter(void);
extern void clickleave(void);
extern void gadgetaborted(struct _Client *c);
extern void gadgetclicked(struct _Client *c, Window w, XEvent *e);
extern void gadgetunclicked(struct _Client *c, XEvent *e);
extern void lowertopmostclient(struct _Scrn *scr);
extern void raisebottommostclient(struct _Scrn *scr);
extern void raiselowerclient(struct _Client *, int);
extern void redraw(struct _Client *, Window);
extern void redrawclient(struct _Client *);
extern void reparent(struct _Client *);
extern void reshape_frame(struct _Client *c);
extern void resizeclientwindow(struct _Client *c, int, int);
