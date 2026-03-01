#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;

struct timeval {
  long tv_sec;
  long tv_usec;
};
#endif

struct _Scrn;

XEvent get_drag_event(void);
XEvent get_next_event(void);
Bool grab_for_motion(Window w, Window confine, Cursor curs, Time time, int x_root, int y_root);
void add_fd_to_set(int fd);
void compress_motion(XEvent *event);
void remove_fd_from_set(int fd);
void wberror(struct _Scrn *s, char *message);

void call_call_out(void);
void call_out(int howlong_s, int howlong_u, void (*what)(void *), void *with);
void remove_call_out(void (*what)(void *), void *with);
void fill_in_call_out(struct timeval *tv);

extern int max_fd;
extern fd_set master_fd_set;
extern struct coevent {
  struct coevent *next;
  struct timeval when;
  void (*what)(void *);
  void *with;
} *eventlist;
