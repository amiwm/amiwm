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

#include "gadget_textinput.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

struct gadget_textinput *
gadget_textinput_create(Display *dpy, struct DrawInfo *dri,
    GC gc, Window mainwin, int x, int y, int width, int height,
    int textsize)
{
	struct gadget_textinput *b;

	b = calloc(1, sizeof(*b));
	if (b == NULL)
		return (NULL);
	b->dpy = dpy;
	b->dri = dri;
	b->gc = gc;
	b->x = x;
	b->y = y;
	b->width = width;
	b->height = height;
	b->buf = calloc(textsize + 1, sizeof(char));
	if (b->buf == NULL) {
		free(b);
		return (NULL);
	}
	b->cur_x = 6;
	b->size = textsize;
	b->w = XCreateSimpleWindow(dpy, mainwin, x, y,
	    width, height, 0,
	    b->dri->dri_Pens[SHADOWPEN],
	    b->dri->dri_Pens[BACKGROUNDPEN]);

	XSelectInput(dpy, b->w, ExposureMask|ButtonPressMask);

	return (b);
}

#ifdef USE_FONTSETS
void
gadget_textinput_set_xic(struct gadget_textinput *g, XIC xic)
{
	g->xic = xic;
}
#endif

static void
gadget_textinput_repaint_str(struct gadget_textinput *b)
{
	int l, mx=6;

	XSetForeground(b->dpy, b->gc, b->dri->dri_Pens[TEXTPEN]);
	if(b->len > b->left_pos) {
#ifdef USE_FONTSETS
	int w, c;

	for (l=0; l<b->len - b->left_pos; ) {
		c=mbrlen(b->buf+b->left_pos+l, b->len-b->left_pos-l, NULL);
		w=6+XmbTextEscapement(b->dri->dri_FontSet, b->buf+b->left_pos, l+c);
		if(w>b->width-6)
			break;
		mx=w;
		l+=c;
	}

	XmbDrawImageString(b->dpy, b->w, b->dri->dri_FontSet, b->gc,
	    6, 3+b->dri->dri_Ascent,
	    b->buf+b->left_pos, l);
#else
    mx+=XTextWidth(b->dri->dri_Font, b->buf+b->left_pos, l=b->len-b->left_pos);
    while(mx>b->width-6)
      mx-=XTextWidth(b->dri->dri_Font, b->buf+b->left_pos + --l, 1);
    XDrawImageString(b->dpy, b->w, b->gc, 6, 3+b->dri->dri_Ascent,
		     b->buf+b->left_pos, l);
#endif
  }
  XSetForeground(b->dpy, b->gc, b->dri->dri_Pens[BACKGROUNDPEN]);
  XFillRectangle(b->dpy, b->w, b->gc, mx, 3, b->width-mx-6, b->width - 6);
  if(b->selected) {
    if(b->cur_pos<b->len) {
      XSetBackground(b->dpy, b->gc, ~0);
#ifdef USE_FONTSETS
      l=mbrlen(b->buf+b->cur_pos, b->len-b->cur_pos, NULL);
      XmbDrawImageString(b->dpy, b->w, b->dri->dri_FontSet, b->gc, b->cur_x,
			 3+b->dri->dri_Ascent, b->buf+b->cur_pos, l);
#else
      XDrawImageString(b->dpy, b->w, b->gc, cur_x, 3+b->dri->dri_Ascent,
		       b->buf+b->cur_pos, 1);
#endif
      XSetBackground(b->dpy, b->gc, b->dri->dri_Pens[BACKGROUNDPEN]);
    } else {
      XSetForeground(b->dpy, b->gc, ~0);
#ifdef USE_FONTSETS
      XFillRectangle(b->dpy, b->w, b->gc, b->cur_x, 3,
		     XExtentsOfFontSet(b->dri->dri_FontSet)->
		     max_logical_extent.width, b->height - 6);
#else
      XFillRectangle(b->dpy, b->w, b->gc, cur_x, 3,
		     b->dri->dri_Font->max_bounds.width, b->height - 6);
#endif
    }
  }
}

void
gadget_textinput_repaint(struct gadget_textinput *b)
{

	gadget_textinput_repaint_str(b);

	XSetForeground(b->dpy, b->gc, b->dri->dri_Pens[SHINEPEN]);
	XDrawLine(b->dpy, b->w, b->gc, 0, b->height-1, 0, 0);
	XDrawLine(b->dpy, b->w, b->gc, 0, 0, b->width-2, 0);
	XDrawLine(b->dpy, b->w, b->gc, 3, b->height-2, b->width-4,
	    b->height-2);
	XDrawLine(b->dpy, b->w, b->gc, b->width-4, b->height-2, b->width-4, 2);
	XDrawLine(b->dpy, b->w, b->gc, 1, 1, 1, b->height-2);
	XDrawLine(b->dpy, b->w, b->gc, b->width-3, 1, b->width-3, b->height-2);
	XSetForeground(b->dpy, b->gc, b->dri->dri_Pens[SHADOWPEN]);
	XDrawLine(b->dpy, b->w, b->gc, 1, b->height-1, b->width-1,
	    b->height-1);
	XDrawLine(b->dpy, b->w, b->gc, b->width-1, b->height-1, b->width-1, 0);
	XDrawLine(b->dpy, b->w, b->gc, 3, b->height-3, 3, 1);
	XDrawLine(b->dpy, b->w, b->gc, 3, 1, b->width-4, 1);
	XDrawLine(b->dpy, b->w, b->gc, 2, 1, 2, b->height-2);
	XDrawLine(b->dpy, b->w, b->gc, b->width-2, 1, b->width-2, b->height-2);
}

void
gadget_textinput_free(struct gadget_textinput *b)
{
	if (b->buf)
		free(b->buf);
	XDestroyWindow(b->dpy, b->w);
	free(b);
}

void
gadget_textinput_keyevent(struct gadget_textinput *b, XKeyEvent *e)
{
#ifdef USE_FONTSETS
  Status stat;
#else
  static XComposeStatus stat;
#endif
  KeySym ks;
  char buf[256];
  int x, i, n;
#ifndef USE_FONTSETS
  n=XLookupString(e, buf, sizeof(buf), &ks, &stat);
#else
  n=XmbLookupString(b->xic, e, buf, sizeof(buf), &ks, &stat);
  if(stat == XLookupKeySym || stat == XLookupBoth)
#endif
  switch(ks) {
  case XK_Return:
  case XK_Linefeed:
    b->crlf = 1;
    break;
  case XK_Left:
    if(b->cur_pos) {
#ifdef USE_FONTSETS
      int p=b->cur_pos;
//      int z;
      while(p>0) {
	--p;
	if(((int)mbrlen(b->buf+p, b->cur_pos-p, NULL))>0) {
	  b->cur_pos=p;
	  break;
	}
      }
#else
      --cur_pos;
#endif
    }
    break;
  case XK_Right:
    if(b->cur_pos<b->len) {
#ifdef USE_FONTSETS
      int l=mbrlen(b->buf+b->cur_pos, b->len-b->cur_pos, NULL);
      if(l>0)
	b->cur_pos+=l;
#else
      b->cur_pos++;
#endif
    }
    break;
  case XK_Begin:
    b->cur_pos=0;
    break;
  case XK_End:
    b->cur_pos=b->len;
    break;
  case XK_Delete:
    if(b->cur_pos<b->len) {
      int l=1;
#ifdef USE_FONTSETS
      l=mbrlen(b->buf+b->cur_pos, b->len-b->cur_pos, NULL);
      if(l<=0)
	break;
#endif
      b->len-=l;
      for(x=b->cur_pos; x<b->len; x++)
	b->buf[x]=b->buf[x+l];
      b->buf[x] = 0;
    } else XBell(b->dpy, 100);
    break;
  case XK_BackSpace:
    if(b->cur_pos>0) {
      int l=1;
#ifdef USE_FONTSETS
      int p=b->cur_pos;
      while(p>0) {
	--p;
	if(((int)mbrlen(b->buf+p, b->len-p, NULL))>0) {
	  l= b->cur_pos - p;
	  break;
	}
      }
#endif
      b->len -= l;
      for(x=(b->cur_pos-=l); x<b->len; x++)
	b->buf[x]=b->buf[x+l];
      b->buf[x] = 0;
    } else XBell(b->dpy, 100);
    break;
#ifdef USE_FONTSETS
  default:
    if(stat == XLookupBoth)
      stat = XLookupChars;
  }
  if(stat == XLookupChars) {
#else
  default:
#endif
    for(i=0; i<n && b->len<b->size-1; i++) {
      for(x=b->len; x>b->cur_pos; --x)
	b->buf[x]=b->buf[x-1];
      b->buf[b->cur_pos++]=buf[i];
      b->len++;
    }
    if(i<n)
      XBell(b->dpy, 100);
  }
  if(b->cur_pos<b->left_pos)
    b->left_pos=b->cur_pos;
  b->cur_x=6;
#ifdef USE_FONTSETS
  if(b->cur_pos>b->left_pos)
    b->cur_x+=XmbTextEscapement(b->dri->dri_FontSet, b->buf+b->left_pos,
      b->cur_pos-b->left_pos);
  if(b->cur_pos < b->len) {
    int l=mbrlen(b->buf+b->cur_pos, b->len-b->cur_pos, NULL);
    x=XmbTextEscapement(b->dri->dri_FontSet, b->buf+b->cur_pos, l);
  } else
    x=XExtentsOfFontSet(b->dri->dri_FontSet)->max_logical_extent.width;
#else
  if(b->cur_pos > b->left_pos)
    b->cur_x+=XTextWidth(b->dri->dri_Font, b->buf+b->left_pos, b->cur_pos-b->left_pos);
  if(b->cur_pos<b->buf_len)
    x=XTextWidth(b->dri->dri_Font, b->buf+b->cur_pos, 1);
  else
    x=b->dri->dri_Font->max_bounds.width;
#endif
  if((x+=b->cur_x-(b->width-6))>0) {
    b->cur_x-=x;
    while(x>0) {
#ifdef USE_FONTSETS
      int l=mbrlen(b->buf+b->left_pos, b->len-b->left_pos, NULL);
      x-=XmbTextEscapement(b->dri->dri_FontSet, b->buf+b->left_pos, l);
      b->left_pos += l;
#else
      x-=XTextWidth(b->dri->dri_Font, b->buf+b->left_pos++, 1);
#endif
    }
    b->cur_x+=x;
  }
}


void
gadget_textinput_buttonevent(struct gadget_textinput *b, XButtonEvent *e)
{
  int w, l=1;
  b->cur_pos=b->left_pos;
  b->cur_x=6;
  while(b->cur_x<e->x && b->cur_pos<b->len) {
#ifdef USE_FONTSETS
    l=mbrlen(b->buf+b->cur_pos, b->len-b->cur_pos, NULL);
    if(l<=0)
      break;
    w=XmbTextEscapement(b->dri->dri_FontSet, b->buf+b->cur_pos, l);
#else
    w=XTextWidth(b->dri->dri_Font, b->buf+b->cur_pos, 1);
#endif
    if(b->cur_x+w>e->x)
      break;
    b->cur_x+=w;
    b->cur_pos+=l;
  }
}

#if 0

void strbutton(XButtonEvent *e)
{
  refresh_str();
}

#endif

void
gadget_textinput_selected(struct gadget_textinput *b, int selected)
{
	b->selected = selected;
}

int
gadget_textinput_crlf(struct gadget_textinput *b)
{
	return (b->crlf);
}
