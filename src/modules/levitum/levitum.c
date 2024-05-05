#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/log.h>
#include <px4_platform_common/tasks.h>
#include <px4_platform_common/posix.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <math.h>

#include <uORB/uORB.h>
#include <uORB/topics/fuel_cell.h>

__EXPORT int levitum_main(int argc, char *argv[]);

int levitum_main(int argc, char *argv[])
{
	PX4_INFO("Hello Levitum!");
	PX4_INFO("exiting");

	return 0;
}
