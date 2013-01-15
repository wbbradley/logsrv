#include <assert.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include "utils.h"
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <iomanip>
#include "logger_decls.h"

#define case_error(error) case error: error_string = #error; break

bool check_errno()
{
	int err = errno;
	if (err == 0)
		return true;
	const char *error_string = "unknown";
	switch (err)
	{
		case_error(EACCES);
		case_error(EAFNOSUPPORT);
		case_error(EISCONN);
		case_error(EMFILE);
		case_error(ENFILE);
		case_error(ENOBUFS);
		case_error(ENOMEM);
		case_error(EPROTO);
		case_error(EHOSTDOWN);
		case_error(EHOSTUNREACH);
		case_error(ENETUNREACH);
		case_error(EPROTONOSUPPORT);
		case_error(EPROTOTYPE);
		case_error(EDQUOT);
		case_error(EAGAIN);
		case_error(EBADF);
		case_error(ECONNRESET);
		case_error(EFAULT);
		case_error(EINTR);
		case_error(EINVAL);
		case_error(ENETDOWN);
		case_error(ENOTCONN);
		case_error(ENOTSOCK);
		case_error(EOPNOTSUPP);
		case_error(ETIMEDOUT);
		case_error(EMSGSIZE);
		case_error(ECONNREFUSED);
	};
	if (err == -1)
		err = errno;

	log(log_info, "check_errno : %s %s\n", error_string, strerror(err));

	return false;
}

double get_current_time()
{
	timeval tv;
	gettimeofday(&tv, NULL);
	double time_now = tv.tv_sec + (double(tv.tv_usec) / 1000000.0);
	return time_now;
}

