#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <errno.h>
#ifdef USE_FONTSETS
#include <locale.h>
#include <wchar.h>
#endif

#include "drawinfo.h"

#include "gadget_button.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

/*
 * This is the button code from the executecmd.c tool.
 */

struct gadget_button *
gadget_button_init(Display *dpy, struct DrawInfo *dri, GC gc, Window mainwin,
    int x, int y, int butw, int buth)
{
	struct gadget_button *b;

	b = calloc(1, sizeof(*b));
	if (b == NULL) {
		return (NULL);
	}
	b->dpy = dpy;
	b->dri = dri;
	b->x = x;
	b->y = y;
	b->butw = butw;
	b->buth = buth;
	b->txt = strdup("");
	b->gc = gc;

	b->w = XCreateSimpleWindow(dpy, mainwin,
	    x, y,
	    butw, /* width */
	    buth, /* height */
	    0, /* depth */
	    dri->dri_Pens[SHADOWPEN],
	    dri->dri_Pens[BACKGROUNDPEN]);

	XSelectInput(dpy, b->w, ExposureMask | ButtonPressMask
	    | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask);

	return (b);
}

void
gadget_button_set_text(struct gadget_button *b, const char *txt)
{
	if (b->txt != NULL)
		free(b->txt);
	b->txt = strdup(txt);
}

void
gadget_button_refresh(struct gadget_button *b)
{
	int fh = b->dri->dri_Ascent + b->dri->dri_Descent;
	int h = fh + (2 * BUT_VSPACE);
	int l=strlen(b->txt);
#ifdef USE_FONTSETS
	int tw = XmbTextEscapement(b->dri->dri_FontSet, b->txt, l);
#else
	int tw = XTextWidth(b->dri->dri_Font, b->txt, l);
#endif
	XSetForeground(b->dpy, b->gc, b->dri->dri_Pens[TEXTPEN]);
#ifdef USE_FONTSETS
	XmbDrawString(b->dpy, b->w, b->dri->dri_FontSet, b->gc,
	    (b->butw-tw)>>1, b->dri->dri_Ascent+BUT_VSPACE, b->txt, l);
#else
	XDrawString(b->dpy, b->w, b->gc, (b->butw-tw)>>1,
	    b->dri->dri_Ascent+BUT_VSPACE, b->txt, l);
#endif
	XSetForeground(b->dpy, b->gc,
	    b->dri->dri_Pens[b->depressed ? SHADOWPEN:SHINEPEN]);

	XDrawLine(b->dpy, b->w, b->gc, 0, 0, b->butw-2, 0);
	XDrawLine(b->dpy, b->w, b->gc, 0, 0, 0, h-2);
	XSetForeground(b->dpy, b->gc,
	    b->dri->dri_Pens[b->depressed ? SHINEPEN:SHADOWPEN]);

	XDrawLine(b->dpy, b->w, b->gc, 1, h-1, b->butw-1, h-1);
	XDrawLine(b->dpy, b->w, b->gc, b->butw-1, 1, b->butw-1, h-1);
	XSetForeground(b->dpy, b->gc, b->dri->dri_Pens[BACKGROUNDPEN]);
	XDrawPoint(b->dpy, b->w, b->gc, b->butw-1, 0);
	XDrawPoint(b->dpy, b->w, b->gc, 0, h-1);
}

void
gadget_button_set_depressed(struct gadget_button *b, int depressed)
{
	b->depressed = depressed;
}

void
gadget_button_toggle(struct gadget_button *b)
{
	int pen;

	pen = (b->depressed) ? FILLPEN : BACKGROUNDPEN;


	XSetWindowBackground(b->dpy, b->w, b->dri->dri_Pens[pen]);
	XClearWindow(b->dpy, b->w);
	gadget_button_refresh(b);
}

void
gadget_button_free(struct gadget_button *b)
{
	XDestroyWindow(b->dpy, b->w);
	free(b->txt);
	free(b);
}
