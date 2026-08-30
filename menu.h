struct Item;
struct _Scrn;
struct module;

extern void createmenubar(void);
extern void disown_item_chain(struct module *m, struct Item *i);
extern void drag_menu(struct _Scrn *s, Time time);
extern void menuaction(struct Item *i, struct Item *si);
extern void redrawmenubar(struct _Scrn *, Window, Bool depthbtn_pressed);
extern struct Item *getitembyhotkey(KeySym);
extern struct Item *own_items(struct module *m, struct _Scrn *s, int menu,
                              int item, int sub, struct Item *c);
