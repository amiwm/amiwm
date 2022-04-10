#ifndef	__LIBAMI_GADGET_TEXTBOX_H__
#define	__LIBAMI_GADGET_TEXTBOX_H__

// These are for the text box widget
#define TXT_HSPACE   48
#define TXT_TOPSPACE 4
#define TXT_MIDSPACE 3
#define TXT_BOTSPACE 4

struct gadget_textbox_line {
	struct gadget_textbox_line *next;
	const char *text;
	int l, w, h;
};

struct gadget_textbox {
	Display *dpy;
	struct DrawInfo *dri;
	GC gc;
	Window w;
	int x, y;
	int width, height;

	struct gadget_textbox_line *firstline, *lastline;
};

extern	struct gadget_textbox *gadget_textbox_create(Display *dpy,
	    struct DrawInfo *dri, GC gc, Window mainwin, int x, int y,
	    int width, int height);
extern	void gadget_textbox_free(struct gadget_textbox *g);
extern	struct gadget_textbox_line * gadget_textbox_addline(
	    struct gadget_textbox *g, const char *text);
extern	void gadget_textbox_refresh(struct gadget_textbox *g);

#endif	/* __LIBAMI_GADGET_TEXTBOX_H__ */
