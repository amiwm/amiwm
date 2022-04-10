#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drawinfo.h"
#include "libami.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

#include "gadget_textbox.h"

#if 0
struct gadget_textbox_line {
	struct line *next;
	const char *text;
	int l, w, h;
};
#endif

#if 0
struct gadget_textbox {
	struct gadget_textbox_line *firstline, *lastline;
};
#endif


struct gadget_textbox *
gadget_textbox_create(Display *dpy, struct DrawInfo *dri, GC gc,
    Window mainwin, int x, int y, int width, int height)
{
	struct gadget_textbox *g;

	g = calloc(1, sizeof(*g));
	if (g == NULL) {
		return (NULL);
	}
	g->dpy = dpy;
	g->dri = dri;
	g->gc = gc;
	g->x = x;
	g->y = y;
	g->width = width;
	g->height = height;

	g->w = XCreateSimpleWindow(g->dpy, mainwin,
	    g->x, g->y,
	    g->width, g->height,
	    0,
	    g->dri->dri_Pens[SHADOWPEN],
	    g->dri->dri_Pens[BACKGROUNDPEN]);
	XSelectInput(g->dpy, g->w, ExposureMask);

	return (g);
}

void
gadget_textbox_free(struct gadget_textbox *g)
{
	/* XXX TODO */
}

struct gadget_textbox_line *
gadget_textbox_addline(struct gadget_textbox *g, const char *text)
{
	struct gadget_textbox_line *l;

	l = calloc(1, sizeof(*l));
	if (l == NULL) {
		return (NULL);
	}
	if(g->lastline)
		g->lastline->next = l;
	else
		g->firstline = l;
	g->lastline = l;
	l->text = strdup(text);
	l->l = strlen(text);
#ifdef USE_FONTSETS
	l->w = XmbTextEscapement(g->dri->dri_FontSet, l->text, l->l);
#else
	l->w = XTextWidth(g->dri->dri_Font, l->text, l->l);
#endif
	l->h = g->dri->dri_Ascent + g->dri->dri_Descent;

	return (l);
}

void
gadget_textbox_refresh(struct gadget_textbox *g)
{
  // This is OBVIOUSLY the wrong value for x here, but let's get it going
  int x = TXT_HSPACE / 2;
  int y = ((g->dri->dri_Ascent+g->dri->dri_Descent)>>1)+g->dri->dri_Ascent;

  struct gadget_textbox_line *l;

  /* Draw the bounding box */
  XSetForeground(g->dpy, g->gc, g->dri->dri_Pens[SHADOWPEN]);
  XDrawLine(g->dpy, g->w, g->gc, 0, 0, g->width-2, 0);
  XDrawLine(g->dpy, g->w, g->gc, 0, 0, 0, g->height-2);

  XSetForeground(g->dpy, g->gc, g->dri->dri_Pens[SHINEPEN]);
  XDrawLine(g->dpy, g->w, g->gc, 0, g->height-1, g->width-1, g->height-1);
  XDrawLine(g->dpy, g->w, g->gc, g->width-1, 0, g->width-1, g->height-1);

  /* Draw text lines */
  XSetForeground(g->dpy, g->gc, g->dri->dri_Pens[TEXTPEN]);
  for(l = g->firstline; l; l=l->next) {
#ifdef USE_FONTSETS
    XmbDrawString(g->dpy, g->w, g->dri->dri_FontSet, g->gc,
      x, y, l->text, l->l);
#else
    XDrawString(g->dpy, g->w, g->gc, x, y, l->text, l->l);
#endif
    y+=g->dri->dri_Ascent + g->dri->dri_Descent;
  }
}
