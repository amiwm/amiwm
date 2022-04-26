#include <stdio.h>
#include <string.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>

#include <machine/apm_bios.h>

#define APM_DEV "/dev/apm"

#include "libami.h"

/*
 * Test battery module for FreeBSD, using APM.
 */

void docmd(XEvent *e, void *callback)
{
  ((void (*)(Window))callback)(e->xany.window);
}

static char *progname;
static int apm_fd = -1;

static bool
get_apm_info(void)
{
	int ret;
	struct apm_info info;

	ret = ioctl(apm_fd, APMIO_GETINFO, &info);
	if (ret < 0) {
		warn("ioctl (APMIO_GETINFO)");
		return false;
	}

	printf("Battery life: %d\n", info.ai_batt_life);
	printf("Battery time: %d\n", info.ai_batt_time);
	printf("Battery AC: %d\n", info.ai_acline);

	md_update_battery(info.ai_batt_life, info.ai_batt_time,
	    info.ai_acline);

	return true;
}

int main(int argc, char *argv[])
{
  char *arg=md_init(argc, argv);

  progname=argv[0];

  apm_fd = open(APM_DEV, O_RDONLY);
  if (apm_fd < 0) {
    err(127, "open");
  }

  /*
   * XXX TODO: how do I actually get this to run once
   * a second in the main loop?
   */
  get_apm_info();

  md_main_loop();

  close(apm_fd);
  return 0;
}
