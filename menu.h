struct Item;
struct _Scrn;

extern void drag_menu(struct _Scrn *);
extern void menuaction(struct Item *, struct Item *);
extern void redrawmenubar(struct _Scrn *, Window);
extern struct Item *getitembyhotkey(KeySym key);
