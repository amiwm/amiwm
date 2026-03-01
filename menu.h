struct Item;
struct _Scrn;
struct module;

extern void drag_menu(struct _Scrn *);
extern void menuaction(struct Item *, struct Item *);
extern void redrawmenubar(struct _Scrn *, Window);
extern struct Item *getitembyhotkey(KeySym key);

extern void disown_item_chain(struct module *m, struct Item *i);
extern struct Item *own_items(struct module *m, struct _Scrn *s,
		       int menu, int item, int sub, struct Item *c);
