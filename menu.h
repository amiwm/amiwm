#ifndef MENU_H
#define MENU_H

#include "screen.h"

struct Item;

void drag_menu(Scrn *);
void menuaction(struct Item *, struct Item *);
void redrawmenubar(Scrn *, Window);
struct Item *getitembyhotkey(KeySym key);

#endif
