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
#include "gadget_textinput.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

#define MAX_CMD_CHARS 256
#define VISIBLE_CMD_CHARS 35

static const char ok_txt[]="Ok", cancel_txt[]="Cancel";
static const char enter_txt[]="Enter Command and its Arguments:";
static const char cmd_txt[]="Command:";

static int selected=0, depressed=0, stractive=1;

struct gadget_button *buttons[3];
struct gadget_textinput *text_input;

char *progname;

Display *dpy;

struct DrawInfo dri;

Window root, mainwin;
GC gc;

int strgadw, strgadh;

int fh;
int mainw, mainh, butw;

#ifdef USE_FONTSETS
static XIM xim = (XIM) NULL;
static XIC xic = (XIC) NULL;
#endif

/*
 * Loop through the button list and see if we find
 * a button.  Return 0 if we don't find a button.
 *
 * Yeah, ideally we'd do this in some gadget / intuition
 * UI layer thing, but we're not there yet.
 */
static int
getchoice(Window w)
{
	int i;
	for (i=1; i<3; i++) {
		if (buttons[i]->w == w)
			return i;
	}
	return 0;
}

static void
refresh_main(void)
{
	int w;

	XSetForeground(dpy, gc, dri.dri_Pens[TEXTPEN]);
#ifdef USE_FONTSETS
	XmbDrawString(dpy, mainwin, dri.dri_FontSet, gc, TEXT_SIDE,
	    TOP_SPACE+dri.dri_Ascent, enter_txt, strlen(enter_txt));
#else
	XDrawString(dpy, mainwin, gc, TEXT_SIDE, TOP_SPACE+dri.dri_Ascent,
	    enter_txt, strlen(enter_txt));
#endif
	XSetForeground(dpy, gc, dri.dri_Pens[HIGHLIGHTTEXTPEN]);
#ifdef USE_FONTSETS
	w = XmbTextEscapement(dri.dri_FontSet, cmd_txt, strlen(cmd_txt));
	XmbDrawString(dpy, mainwin, dri.dri_FontSet, gc,
	    mainw-strgadw-w-TEXT_SIDE-BUT_SIDE,
	    TOP_SPACE+fh+INT_SPACE+dri.dri_Ascent,
	    cmd_txt, strlen(cmd_txt));
#else
	w = XTextWidth(dri.dri_Font, cmd_txt, strlen(cmd_txt));
	XDrawString(dpy, mainwin, gc, mainw-strgadw-w-TEXT_SIDE-BUT_SIDE,
	    TOP_SPACE+fh+INT_SPACE+dri.dri_Ascent,
	    cmd_txt, strlen(cmd_txt));
#endif
}

static void
toggle(int c)
{
	if (c == 0)
		return;
	gadget_button_toggle(buttons[c]);
}

static void
abortchoice(void)
{
	if(depressed) {
		depressed=0;
		if (selected > 0) {
			gadget_button_set_depressed(buttons[selected], 0);
		}
		toggle(selected);
	}
	selected = 0;
}

static void
endchoice(void)
{
	int c=selected;

	abortchoice();
	XCloseDisplay(dpy);
	if(c==1) {
		system(text_input->buf);
	}
	exit(0);
}

int
main(int argc, char *argv[])
{
	XWindowAttributes attr;
	static XSizeHints size_hints;
	static XTextProperty txtprop1, txtprop2;
	int w2, c;
#ifdef USE_FONTSETS
	char *p;

	setlocale(LC_CTYPE, "");
#endif
	progname=argv[0];
	if(!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "%s: cannot connect to X server %s\n",
		    progname, XDisplayName(NULL));
		exit(1);
	}

	root = RootWindow(dpy, DefaultScreen(dpy));
	XGetWindowAttributes(dpy, root, &attr);
	init_dri(&dri, dpy, root, attr.colormap, False);

#ifdef USE_FONTSETS
  strgadw=VISIBLE_CMD_CHARS*XExtentsOfFontSet(dri.dri_FontSet)->
    max_logical_extent.width+12;
#else
  strgadw=VISIBLE_CMD_CHARS*dri.dri_Font->max_bounds.width+12;
#endif
  strgadh=(fh=dri.dri_Ascent+dri.dri_Descent)+6;

#ifdef USE_FONTSETS
  butw=XmbTextEscapement(dri.dri_FontSet, ok_txt, strlen(ok_txt))+2*BUT_HSPACE;
  w2=XmbTextEscapement(dri.dri_FontSet, cancel_txt, strlen(cancel_txt))+2*BUT_HSPACE;
#else
  butw=XTextWidth(dri.dri_Font, ok_txt, strlen(ok_txt))+2*BUT_HSPACE;
  w2=XTextWidth(dri.dri_Font, cancel_txt, strlen(cancel_txt))+2*BUT_HSPACE;
#endif
  if(w2>butw)
    butw=w2;

  mainw=2*(BUT_SIDE+butw)+BUT_SIDE;
#ifdef USE_FONTSETS
  w2=XmbTextEscapement(dri.dri_FontSet, enter_txt, strlen(enter_txt))+2*TEXT_SIDE;
#else
  w2=XTextWidth(dri.dri_Font, enter_txt, strlen(enter_txt))+2*TEXT_SIDE;
#endif
  if(w2>mainw)
    mainw=w2;
#ifdef USE_FONTSETS
  w2=strgadw+XmbTextEscapement(dri.dri_FontSet, cmd_txt, strlen(cmd_txt))+
    2*TEXT_SIDE+2*BUT_SIDE+butw;
#else
  w2=strgadw+XTextWidth(dri.dri_Font, cmd_txt, strlen(cmd_txt))+
    2*TEXT_SIDE+2*BUT_SIDE+butw;
#endif
  if(w2>mainw)
    mainw=w2;

  mainh=3*fh+TOP_SPACE+BOT_SPACE+2*INT_SPACE+2*BUT_VSPACE;

#ifdef USE_FONTSETS
  if ((p = XSetLocaleModifiers("@im=none")) != NULL && *p)
    xim = XOpenIM(dpy, NULL, NULL, NULL);
  if (!xim)
    fprintf(stderr, "Failed to open input method.\n");
  else {
    xic = XCreateIC(xim,
		    XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
		    XNClientWindow, mainwin,
		    XNFocusWindow, mainwin,
		    NULL);
    if (!xic)
      fprintf(stderr, "Failed to create input context.\n");
  }
  if (!xic)
    exit(1);
#endif

  /* Main window */
  mainwin=XCreateSimpleWindow(dpy, root, 20, 20, mainw, mainh, 1,
			      dri.dri_Pens[SHADOWPEN],
			      dri.dri_Pens[BACKGROUNDPEN]);
  gc=XCreateGC(dpy, mainwin, 0, NULL);
  XSetBackground(dpy, gc, dri.dri_Pens[BACKGROUNDPEN]);
#ifndef USE_FONTSETS
  XSetFont(dpy, gc, dri.dri_Font->fid);
#endif

  /* Text input window */
  text_input = gadget_textinput_create(dpy, &dri, gc, mainwin,
    mainw-BUT_SIDE-strgadw,
   TOP_SPACE+fh+INT_SPACE-3,
   strgadw, strgadh, 256);
#ifdef USE_FONTSETS
  gadget_textinput_set_xic(text_input, xic);
#endif


  /* Create OK button */
  buttons[1] = gadget_button_init(dpy, &dri, gc, mainwin,
    BUT_SIDE, mainh-BOT_SPACE-2*BUT_VSPACE-fh, /* x, y */
    butw, fh+2*BUT_VSPACE); /* width, height */
  gadget_button_set_text(buttons[1], ok_txt);

  /* Create cancel button */
  buttons[2] = gadget_button_init(dpy, &dri, gc, mainwin,
    mainw-butw-BUT_SIDE, mainh-BOT_SPACE-2*BUT_VSPACE-fh, /* x, y */
    butw, fh+2*BUT_VSPACE); /* width, height */
  gadget_button_set_text(buttons[2], cancel_txt);

  XSelectInput(dpy, mainwin, ExposureMask|KeyPressMask|ButtonPressMask);

  size_hints.flags = PResizeInc;
  txtprop1.value=(unsigned char *)"Execute a File";
  txtprop2.value=(unsigned char *)"ExecuteCmd";
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
#ifdef USE_FONTSETS
    if(!XFilterEvent(&event, mainwin))
#endif
    switch(event.type) {
    case Expose:
      if(!event.xexpose.count) {
	if(event.xexpose.window == mainwin)
	  refresh_main();	
	else if(event.xexpose.window == text_input->w)
	  gadget_textinput_repaint(text_input);
	else if(event.xexpose.window == buttons[1]->w) {
	  gadget_button_refresh(buttons[1]);
	} else if(event.xexpose.window == buttons[2]->w) {
	  gadget_button_refresh(buttons[2]);
	}
      }
    case LeaveNotify:
      if (depressed) {
        if ((selected > 0) && event.xcrossing.window == buttons[selected]->w) {
          depressed = 0;
          gadget_button_set_depressed(buttons[selected], 0);
          toggle(selected);
        }
      }

      break;
    case EnterNotify:
      if((!depressed) && selected && event.xcrossing.window == buttons[selected]->w) {
        depressed = 1;
        gadget_button_set_depressed(buttons[selected], 1);
        toggle(selected);
      }
      break;
    case ButtonPress:
      if(event.xbutton.button==Button1) {
	if(stractive && event.xbutton.window!= text_input->w) {
	  stractive=0;
	  gadget_textinput_selected(text_input, 0);
	  gadget_textinput_repaint(text_input);
	}
	if((c=getchoice(event.xbutton.window))) {
	  abortchoice();
	  depressed = 1;
	  selected = c;
	  if (selected > 0) {
	    gadget_button_set_depressed(buttons[selected], 1);
	  }
	  toggle(selected);
	} else if(event.xbutton.window== text_input->w) {
	  gadget_textinput_selected(text_input, 1);
	  gadget_textinput_buttonevent(text_input, &event.xbutton);
	  gadget_textinput_repaint(text_input);
	}
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
    case KeyPress:
      if(stractive) {
	gadget_textinput_keyevent(text_input, &event.xkey);
	gadget_textinput_repaint(text_input);
	if (gadget_textinput_crlf(text_input)) {
	  selected = 1;
	  endchoice();
	  exit(1);
	}
      }
    }
  }
}
