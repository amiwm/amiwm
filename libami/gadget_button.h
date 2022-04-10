#ifndef	__LIBAMI__GADGET_BUTTON_H__
#define	__LIBAMI__GADGET_BUTTON_H__

#define BOT_SPACE 4
#define TEXT_SIDE 8
#define BUT_SIDE 12
#define TOP_SPACE 4
#define INT_SPACE 7
#define BUT_VSPACE 2
#define BUT_HSPACE 8

struct gadget_button {
	Display *dpy;
	struct DrawInfo *dri;
	Window w;
	GC gc;
	int x;
	int y;
	int buth;
	int butw;
	char *txt;
	int depressed;
};

extern	struct gadget_button * gadget_button_init(Display *dpy,
	    struct DrawInfo *dri, GC gc, Window mainwin,
	    int x, int y, int butw, int buth);
extern	void gadget_button_set_text(struct gadget_button *b, const char *txt);
extern	void gadget_button_refresh(struct gadget_button *b);
extern	void gadget_button_set_depressed(struct gadget_button *b,
	    int depressed);
extern	void gadget_button_toggle(struct gadget_button *b);
extern	void gadget_button_free(struct gadget_button *b);

#endif	/* __LIBAMI__GADGET_BUTTON_H__ */
