#ifndef	__LIBAMI__GADGET_TEXTINPUT_H__
#define	__LIBAMI__GADGET_TEXTINPUT_H__

struct gadget_textinput {
	Display *dpy;
	struct DrawInfo *dri;
	Window w;
	GC gc;
#ifdef USE_FONTSETS
	XIC xic;
#endif
	int x;
	int y;
	int width;
	int height;

	/* XXX TODO: create a string representation here already */
	char *buf;
	int len;
	int size;

	/* Position of textbox cursor and rendering start */
	int cur_pos;
	int left_pos;
	int cur_x;

	int selected;
	int crlf;
};

extern	struct gadget_textinput * gadget_textinput_create(Display *dpy,
	    struct DrawInfo *dri, GC gc, Window mainwin,
	    int x, int y, int width, int height, int text_size);
#ifdef USE_FONTSETS
extern	void gadget_textinput_set_xic(struct gadget_textinput *g, XIC xic);
#endif
extern	void gadget_textinput_repaint(struct gadget_textinput *b);
extern	void gadget_textinput_free(struct gadget_textinput *b);
extern	void gadget_textinput_keyevent(struct gadget_textinput *b,
	    XKeyEvent *e);
extern	void gadget_textinput_buttonevent(struct gadget_textinput *b,
	    XButtonEvent *e);
extern	void gadget_textinput_selected(struct gadget_textinput *b, int selected);
extern	int gadget_textinput_crlf(struct gadget_textinput *b);

#endif	/* __LIBAMI__GADGET_TEXTINPUT_H__ */
