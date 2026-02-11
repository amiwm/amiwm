#include "client.h"

void reparent(Client *c);
void redraw(Client *c, Window w);
void redrawclient(Client *c);
void resizeclientwindow(Client *c, int width, int height);
void reshape_frame(Client *c);
void raiselowerclient(Client *c, int place);
void click_frame(Client *c, Time time, Window w);
