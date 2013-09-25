#include <unistd.h>
#ifdef ANDROID
#include <sys/reboot.h>
#endif

#include "util.h"

void util_halt(void)
{
#ifdef ANDROID
	reboot(RB_POWER_OFF);
#else
	execlp("shutdown", "shutdown", "-h", "now", NULL);
#endif
	_exit(1);
}
