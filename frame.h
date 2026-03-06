struct _Client;

extern void click_close(struct _Client *c, Time time);
extern void click_depth(struct _Client *c, Time time);
extern void click_iconify(struct _Client *c, Time time);
extern void click_zoom(struct _Client *c, Time time);
extern void raiselowerclient(struct _Client *c, int place);
extern void redraw(struct _Client *c, Window w);
extern void redrawclient(struct _Client *c);
extern void reparent(struct _Client *c);
extern void reshape_frame(struct _Client *c);
extern void resizeclientwindow(struct _Client *c, int width, int height);
