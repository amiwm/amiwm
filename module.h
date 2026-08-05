#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef AMIGAOS
#define fd_set XTransFdset
#undef FD_ZERO
#undef FD_SET
#define FD_ZERO XTransFdZero
#define FD_SET XTransFdSet
#define select XTransSelect
#endif

#define MCMD_NOP 1
#define MCMD_GET_VERSION 2
#define MCMD_SEND_EVENT 4
#define MCMD_SET_BROKER 5
#define MCMD_ROTATE_SCREEN 6
#define MCMD_ADD_KEYGRAB 7
#define MCMD_DEL_KEYGRAB 8
#define MCMD_FRONT 9
#define MCMD_BACK 10
#define MCMD_ICONIFY 11
#define MCMD_CREATEAPPICON 12
#define MCMD_ERRORMSG 14
#define MCMD_SETAPPWINDOW 15
#define MCMD_GETICONDIR 16
#define MCMD_GETICONPALETTE 17
#define MCMD_MANAGEMENU 18
#define MCMD_ROTATE_WINDOW_RAISE 19
#define MCMD_ROTATE_WINDOW_LOWER 20
#define MCMD_UPDATE_BATTERY 21
#define MCMD_WINDOW_MOVE_NEXT_DESKTOP 22

struct mcmd_header {
  XID id;
  int cmd;
  int len;
};

struct mcmd_event {
  unsigned long mask;
  XEvent event;
};

struct mcmd_keygrab {
  struct mcmd_keygrab *next;
  int id;
  struct module *owner;
  int keycode;
  unsigned int modifiers;
};

struct NewAppIcon {
  int x, y;
  Pixmap pm1, pm2, pmm;
  char name[1];
};

struct mcmd_update_battery {
  int battery_time;
  int battery_pct;
  int battery_cap;
  int battery_ac;
  int battery_charging;
};

extern struct module {
  struct module *next;
  int in_fd, out_fd;
  pid_t pid;
  struct mcmd_header mcmd;
  char *in_ptr;
  int in_left;
  int in_phase;
  char *in_buf;
  int in_buf_size;
  struct {
    int exists;
    unsigned long mask;
  } broker;
  struct Item *menuitems;
} *modules;

extern int dispatch_event_to_broker(XEvent *, unsigned long, struct module *);
extern void flushmodules(void);
extern void handle_module_input(fd_set *);
extern void init_modules(void);
extern void mod_menuselect(struct module *, int, int, int);
