#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/select.h>

#include "libami.h"
#include "module.h"
#include "alloc.h"

static int md_in_fd=-1, md_out_fd=-1;

static char *md_name = "";

char *amiwm_version;

Window md_root = None;

static int md_int_len=0;
static char *md_int_buf=NULL;
void (*md_broker_func)(XEvent *, unsigned long);
void (*md_periodic_func)(void);

void md_exit(int signal)
{
  if(md_in_fd>=0)
    close(md_in_fd);
  if(md_out_fd>=0)
    close(md_out_fd);
  exit(0);
}

static int md_write(void *ptr, int len)
{
  char *p=ptr;
  int r, tot=0;
  while(len>0) {
    if((r=write(md_out_fd, p, len))<0) {
      if(errno==EINTR)
        continue;
      else
        return r;
    }
    if(!r)
      return tot;
    tot+=r;
    p+=r;
    len-=r;
  }
  return tot;
}

/*
 * Wait until the read FD is ready, or timeout (5 seconds.)
 *
 * Returns:
 * + 1 if OK
 * + 0 if timeout
 * < 0 if error
 */
static int
md_wait_read_fd(void)
{
  fd_set readfds;
  struct timeval tv;
  int ret;

  FD_ZERO(&readfds);
  FD_SET(md_in_fd, &readfds);
  tv.tv_sec = 5;
  tv.tv_usec = 0;

  ret = select(md_in_fd + 1, &readfds, NULL, NULL, &tv);
  if (ret == 0) {
    return (0);
  }
  if (ret < 0) {
    /*
     * Note: this happens during things like system suspend/resume
     * on FreeBSD.
     */
    if (errno == EINTR) {
        return (0);
    }
    return (-1);
  }
  if (FD_ISSET(md_in_fd, &readfds)) {
    return (1);
  }

  /* FD wasn't set; just return 0 */
  return (0);
}

/*
 * Read from the input file descriptor until len; ,populate
 * our buffer.
 *
 * Return total read, 0 on timeout, else -1 on error.
 */
static int md_read(void *ptr, int len, int block)
{
  char *p=ptr;
  int r, tot=0;

  while (len > 0) {

    /* Wait until the socket is ready, or timeout */
    r = md_wait_read_fd();
    if (r < 0) {
      return (-1);
    }

    /*
     * Note: If we've read /anything/, then we just keep
     * going until we're done.  Otherwise we'll exit
     * out here with a partial read and things will
     * go sideways.
     *
     * If we're not blocking and select timed out,
     * return timeout.
     */
    if ((tot == 0) && (block == 0) && (r == 0)) {
      return (0);
    }

    /*
     * Try to read some data.  Go back around again
     * if we hit EINTR/EWOULDBLOCK.
     *
     * If we hit EOF then that's an error.
     */
    r = read(md_in_fd, p, len);

    /* Error */
    if (r < 0) {
      if ((errno == EINTR) || (errno == EWOULDBLOCK)) {
        continue;
      } else {
       return (-1);
      }
    }

    /*
     * EOF and didn't read anything? md_exit() like
     * the old code did.
     */
    if ((r == 0) && (tot == 0)) {
      md_exit(0);
    }

    /*
     * EOF, but we read data, so at least return what
     * we did read.
     */
    if (r == 0) {
      return (tot);
    }

    /* r > 0 here */

    tot+=r;
    p+=r;
    len-=r;
  }

  return (tot);
}

/*
 * Read in "len" bytes from the window manager command path
 * into md_int_buf.
 */
static int md_int_load(int len)
{
  if(len>=md_int_len) {
    if(md_int_buf!=NULL)
      md_int_buf=realloc(md_int_buf, md_int_len=len+1);
    else
      md_int_buf=malloc(md_int_len=len+1);
  }

  md_int_buf[len]='\0';
  return md_read(md_int_buf, len, 1);
}

static struct md_queued_event {
  struct md_queued_event *next;
  struct mcmd_event e;
} *event_head=NULL, *event_tail=NULL;

/*
 * Process queued XEvents from the window manager.
 *
 * The window manager pushes subscribed XEvents down to
 * modules and this pulls them out of the queue and
 * calls md_broker_func() on each of them.
 */
void md_process_queued_events()
{
  struct md_queued_event *e;

  while((e=event_head)) {
    event_head=e->next;
    md_broker_func(&e->e.event, e->e.mask);
    free(e);
  }
}

/*
 * Enqueue an mcmd event into the event queue.
 *
 * This is called when there's an XEvent being queued
 * from the window manager to the module.
 */
static void md_enqueue(struct mcmd_event *e)
{
  struct md_queued_event *qe=malloc(sizeof(struct md_queued_event));
  if(qe) {
    qe->e=*e;
    qe->next=NULL;
    if(event_head) {
      event_tail->next=qe;
      event_tail=qe;
    } else {
      event_head=event_tail=qe;
    }
  }
}

/*
 * Read an async XEvent from the window manager.
 *
 * This is called by md_handle_input() to read an XEvent.
 * The "I'm an Xevent" marker is ~len, so it's de-inverted
 * and then a subsequent len field match must match it.
 */
static int md_get_async(int len)
{
  if(md_int_load(len)!=len)
    return -1;
  if(md_broker_func)
    md_enqueue((struct mcmd_event *)md_int_buf);
  return 1;
}

/*
 * Read input from the window manager.
 *
 * This reads two chunks - the size of the request,
 * and then the request itself.
 *
 * Negative request lengths are treated special - they're
 * treated as XEvents thrown into the input stream.
 *
 * Returns >1 if got input, 0 if timed out, < 0 if error.
 */
int md_handle_input(void)
{
  int res, ret;

  /* Read the length of the request, don't block. */
  ret = md_read(&res, sizeof(res), 0);

  /* Timeout? */
  if (ret == 0) {
    return (0);
  }

  /* Error? */
  if (ret < 0) {
    return (-1);
  }

  /* Read size doesn't match request size? */
  if (ret != sizeof(res)) {
    return (-1);
  }

  if(res>=0) {
    if(!res)
      return 0;
    /* Read the command */
    md_int_load(res);
    return 0;
  } else {
    /* Negative length; treat as an XEvent */
    res=~res;
    if(!res)
      return 0;
    return md_get_async(res);
  }
}

/*
 * Send a command from the module back to the window manager.
 *
 * This sends a request up to the window manager and then reads the
 * response to return.  If asynchronous XEvents occur in the reply
 * stream then those are enqueued via md_get_async().
 *
 * If there is a response, buffer is set to a memory buffer containing it.
 * It is thus up to the caller to free it.
 */
int md_command(XID id, int cmd, void *data, int data_len, char **buffer)
{
  int res;
  struct mcmd_header mcmd;

  *buffer=NULL;

  mcmd.id = id;
  mcmd.cmd = cmd;
  mcmd.len = data_len;

  /*
   * Send header, read response code.
   */
  if(md_write(&mcmd, sizeof(mcmd))!=sizeof(mcmd) ||
     md_write(data, data_len)!=data_len ||
     md_read(&res, sizeof(res), 1)!=sizeof(res))
    return -1;

  /*
   * If the response code is negative (well, less than -1)
   * then its treated as an async XEvent.  So, queue that
   * and keep reading for the response code.
   */
  while(res<-1) {
    md_get_async(~res);
    if(md_read(&res, sizeof(res), 1)!=sizeof(res))
      return -1;
  }

  /*
   * If the response code is >0, then allocate a buffer
   * of a suitable size and read the response into the buffer.
   */
  if(res>0) {
    *buffer=malloc(res);
    if(md_read(*buffer, res, 1)!=res)
      return -1;
  }

  /*
   * Return the response size.
   */
  return res;
}

int md_command0(XID id, int cmd, void *data, int data_len)
{
  char *ptr=NULL;
  int res=md_command(id, cmd, data, data_len, &ptr);
  if(ptr) free(ptr);
  return res;
}

int md_command00(XID id, int cmd)
{
  return md_command0(id, cmd, NULL, 0);
}

static void md_fail()
{
  fprintf(stderr, "%s: cannot establish connection to amiwm\n", md_name);
  exit(1);
}

Display *md_display()
{
  static Display *dpy=NULL;
  if(!dpy)
    dpy=XOpenDisplay(NULL);
  return dpy;
}

/*
 * make the fd blocking or non-blocking.
 */
static int
md_fd_nonblocking(int fd, int nb)
{
  int ret, val;

  val = fcntl(fd, F_GETFD);
  if (nb) {
    ret = fcntl(fd, F_SETFD, val | O_NONBLOCK);
  } else {
    ret = fcntl(fd, F_SETFD, val & ~O_NONBLOCK);
  }
  return (ret == 0);
}

char *md_init(int argc, char *argv[])
{
  if(argc>0)
    md_name=argv[0];
  if(argc>2) {
    md_in_fd=strtol(argv[1], NULL, 0);
    md_out_fd=strtol(argv[2], NULL, 0);
  } else
    md_fail();

  signal(SIGHUP, md_exit);
  signal(SIGPIPE, md_exit);

  if(argc>3)
    md_root=strtol(argv[3], NULL, 0);
  else
    md_root=None;

  if(md_command(None, MCMD_GET_VERSION, NULL, 0, &amiwm_version)<=0)
    md_fail();

  md_fd_nonblocking(md_in_fd, 1);

  return (argc>4? argv[4]:NULL);
}

void
md_main_loop()
{
  do {
    if (md_periodic_func != NULL) {
      md_periodic_func();
    }

    /* Process async XEvent events that have been read */
    md_process_queued_events();

    /* Loop over, reading input events */
  } while(md_handle_input()>=0);
}

int md_connection_number()
{
  return md_in_fd;
}
