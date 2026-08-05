struct Item;
struct _Scrn;
struct module;

extern void createmenubar(void);
extern void disown_item_chain(struct module *m, struct Item *i);
extern void menu_off(void);
extern void menu_on(void);
extern void menuaction(struct Item *i, struct Item *si);
extern void menubar_enter(Window);
extern void menubar_leave(Window);
extern void redrawmenubar(struct _Scrn *, Window);
extern struct Item *getitembyhotkey(KeySym);
extern struct Item *own_items(struct module *m, struct _Scrn *s, int menu,
                              int item, int sub, struct Item *c);

extern struct _Scrn *mbdclick, *mbdscr;
