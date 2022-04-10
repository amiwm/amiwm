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

// These are used for button spacing, not the button itself
#define BUT_BUTSPACE (2*(2+5))
#define BUT_INTSPACE 12
#define BUT_EXTSPACE 4

#include "gadget_button.h"
#include "gadget_textbox.h"

struct choice {
	struct choice *next;
	const char *text;
	int l, w;
	struct gadget_button *b;
} *firstchoice=NULL, *lastchoice=NULL;

struct line {
	struct line *next;
	const char *text;
	int l, w, h;
} *firstline=NULL, *lastline=NULL;

Display *dpy;
Window root, mainwin;

//Window textwin;
struct gadget_textbox *g_textbox;

char *progname;
GC gc;
Pixmap stipple;

int totw=0, maxw=0, toth=0, nchoices=0;
int depressed=0;
struct choice *selected=NULL;

struct DrawInfo dri;

struct RDArgs *ra=NULL;

static void
selection(int n)
{
	printf("%d\n", n);
	XDestroyWindow(dpy, mainwin);
	XCloseDisplay(dpy);
	FreeArgs(ra);
	exit(0);
}

static void
*myalloc(size_t s)
{
	void *p=calloc(s,1);
	if(p)
		return p;
	fprintf(stderr, "%s: out of memory!\n", progname);
	FreeArgs(ra);
	exit(1);
}

/*
 * Add a choice to the list of choices, but don't create
 * the button just yet.  This'll be done once all of them
 * are known and the math to create the window sizing/layout
 * can be done.
 */
static void
addchoice(const char *txt)
{
	struct choice *c=myalloc(sizeof(struct choice));
	if(lastchoice)
		lastchoice->next=c;
	else
		firstchoice=c;
	lastchoice=c;

	c->l = strlen(txt);
	c->text = txt;

#ifdef USE_FONTSETS
	totw+=(c->w=XmbTextEscapement(dri.dri_FontSet, c->text, c->l))+BUT_BUTSPACE;
#else
	totw+=(c->w=XTextWidth(dri.dri_Font, c->text, c->l))+BUT_BUTSPACE;
#endif
	nchoices++;
}

/*
 * Add a line to the text box that we can draw.
 */
static void
addline(const char *txt)
{
  struct line *l=myalloc(sizeof(struct line));
  if(lastline)
    lastline->next=l;
  else
    firstline=l;
  lastline=l;
  l->l=strlen(l->text=txt);

  /* These are used to find the size, which we want
   * to use in order to determine how big our window
   * should be.  It's a bit of a chicken/egg problem
   * for now whilst this is figured out - it's also
   * done in gadget_textbox_addline().
   */
#ifdef USE_FONTSETS
  l->w=XmbTextEscapement(dri.dri_FontSet, l->text, l->l);
#else
  l->w=XTextWidth(dri.dri_Font, l->text, l->l);
#endif
  toth+=l->h=dri.dri_Ascent+dri.dri_Descent;
  if(l->w>maxw)
    maxw=l->w;
}

void refresh_choice(struct choice *c)
{
  gadget_button_refresh(c->b);
}

static void
split(char *str, const char *delim, void (*func)(const char *))
{
  char *p;
  if((p=strtok(str, delim)))
    do {
      (*func)(p);
    } while((p=strtok(NULL, delim)));
}

struct choice *getchoice(Window w)
{
  struct choice *c;
  for(c=firstchoice; c; c=c->next)
    if(w == c->b->w)
      return c;
  return NULL;
}

void toggle(struct choice *c)
{
  gadget_button_toggle(c->b);
}

void abortchoice()
{
  if(depressed) {
    depressed=0;
    gadget_button_set_depressed(selected->b, 0);
    toggle(selected);
  }
  selected=NULL;
}

void endchoice()
{
  struct choice *c=selected, *c2=firstchoice;
  int n;

  abortchoice();
  if(c==lastchoice)
    selection(0);
  for(n=1; c2; n++, c2=c2->next)
    if(c2==c)
      selection(n);
  selection(0);
}

int main(int argc, char *argv[])
{
  XWindowAttributes attr;
  static XSizeHints size_hints;
  static XTextProperty txtprop1, txtprop2;
  int x, y, extra=0, n=0;
  struct choice *c;
  struct line *l;
  Argtype array[3], *atp;

  progname=argv[0];
  initargs(argc, argv);
  if(!(ra=ReadArgs((STRPTR)"TITLE/A,BODY/A,GADGETS/M", (LONG *)array, NULL))) {
    PrintFault(IoErr(), (UBYTE *)progname);
    exit(1);
  }
  if(!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "%s: cannot connect to X server %s\n", progname,
	    XDisplayName(NULL));
    FreeArgs(ra);
    exit(1);
  }
  root = RootWindow(dpy, DefaultScreen(dpy));
  XGetWindowAttributes(dpy, root, &attr);
  init_dri(&dri, dpy, root, attr.colormap, False);

  split(array[1].ptr, "\n", addline);
  if((atp=array[2].ptr) != NULL)
    for(; atp->ptr; atp++)
      split(atp->ptr, "|\n", addchoice);

  totw+=BUT_EXTSPACE+BUT_EXTSPACE+BUT_INTSPACE*(nchoices-1);
  toth+=2*(dri.dri_Ascent+dri.dri_Descent)+TXT_TOPSPACE+
    TXT_MIDSPACE+TXT_BOTSPACE+BUT_VSPACE;
  maxw+=TXT_HSPACE+BUT_EXTSPACE+BUT_EXTSPACE;

  if(maxw>totw) {
    extra=maxw-totw;
    totw=maxw;
  }

  mainwin=XCreateSimpleWindow(dpy, root, 0, 0, totw, toth, 1,
			      dri.dri_Pens[SHADOWPEN],
			      dri.dri_Pens[BACKGROUNDPEN]);
  gc=XCreateGC(dpy, mainwin, 0, NULL);
  XSetBackground(dpy, gc, dri.dri_Pens[BACKGROUNDPEN]);
#ifndef USE_FONTSETS
  XSetFont(dpy, gc, dri.dri_Font->fid);
#endif
  stipple=XCreatePixmap(dpy, mainwin, 2, 2, attr.depth);
  XSetForeground(dpy, gc, dri.dri_Pens[BACKGROUNDPEN]);
  XFillRectangle(dpy, stipple, gc, 0, 0, 2, 2);
  XSetForeground(dpy, gc, dri.dri_Pens[SHINEPEN]);
  XDrawPoint(dpy, stipple, gc, 0, 1);
  XDrawPoint(dpy, stipple, gc, 1, 0);
  XSetWindowBackgroundPixmap(dpy, mainwin, stipple);

  g_textbox = gadget_textbox_create(dpy, &dri, gc, mainwin,
    BUT_EXTSPACE,
    TXT_TOPSPACE,
    totw - BUT_EXTSPACE - BUT_EXTSPACE,
    toth-TXT_TOPSPACE- TXT_MIDSPACE-TXT_BOTSPACE-BUT_VSPACE- (dri.dri_Ascent+dri.dri_Descent));
#if 0
  textwin=XCreateSimpleWindow(dpy, mainwin, BUT_EXTSPACE, TXT_TOPSPACE, totw-
			      BUT_EXTSPACE-BUT_EXTSPACE,
			      0, dri.dri_Pens[SHADOWPEN],
			      dri.dri_Pens[BACKGROUNDPEN]);
  XSelectInput(dpy, textwin, ExposureMask);
#endif

  /* Lay out + create buttons */
  x=BUT_EXTSPACE;
  y=toth-TXT_BOTSPACE-(dri.dri_Ascent+dri.dri_Descent)-BUT_VSPACE;
  for(c=firstchoice; c; c=c->next) {
    c->b = gadget_button_init(dpy, &dri, gc, mainwin,
        x + (nchoices == 1 ? (extra >> 1) : n++*extra/(nchoices-1)),
        y,
        c->w + BUT_BUTSPACE,
        // Note: the original code didn't need a +2 here, but
        // when using the ported button gadget it's needed or
        // the bottom of the button isn't shown. Figure out why!
        dri.dri_Ascent+dri.dri_Descent + BUT_VSPACE + 2);
    gadget_button_set_text(c->b, c->text);
    x+=c->w+BUT_BUTSPACE+BUT_INTSPACE;
  }

  /* Lay out + create text box contents */
  for (l = firstline; l; l = l->next) {
    gadget_textbox_addline(g_textbox, l->text);
  }

  size_hints.flags = PResizeInc;
  txtprop1.value=(unsigned char *)array[0].ptr;
  txtprop2.value=(unsigned char *)"RequestChoice";
  txtprop2.encoding=txtprop1.encoding=XA_STRING;
  txtprop2.format=txtprop1.format=8;
  txtprop1.nitems=strlen((char *)txtprop1.value);
  txtprop2.nitems=strlen((char *)txtprop2.value);
  XSetWMProperties(dpy, mainwin, &txtprop1, &txtprop2, argv, argc,
		   &size_hints, NULL, NULL);
  XMapSubwindows(dpy, mainwin);
  XMapRaised(dpy, mainwin);
  for(;;) {
    XEvent event;
    XNextEvent(dpy, &event);
    switch(event.type) {
    case Expose:
      if(!event.xexpose.count) {
	if(event.xexpose.window == g_textbox->w)
	  gadget_textbox_refresh(g_textbox);
	else if((c=getchoice(event.xexpose.window)))
	  refresh_choice(c);
      }
      break;
    case LeaveNotify:
      if(depressed && event.xcrossing.window==selected->b->w) {
	depressed=0;
	gadget_button_set_depressed(selected->b, 0);
	toggle(selected);
      }
      break;
    case EnterNotify:
      if((!depressed) && selected && event.xcrossing.window==selected->b->w) {
	gadget_button_set_depressed(selected->b, 1);
	depressed=1;
	toggle(selected);
      }
      break;
    case ButtonPress:
      if(event.xbutton.button==Button1 &&
	 (c=getchoice(event.xbutton.window))) {
	abortchoice();
	depressed=1;
	gadget_button_set_depressed(c->b, 1);
	toggle(selected=c);
      }
      break;
    case ButtonRelease:
      if(event.xbutton.button==Button1 && selected) {
	if(depressed)
	  endchoice();
	else
	  abortchoice();
      }
      break;
    }
  }
  FreeArgs(ra);
}
