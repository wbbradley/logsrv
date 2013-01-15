#include <assert.h>
#include "disk.h"
#include <stdio.h>
#include <string>
#include <sstream>

bool ensure_directory_exists(const std::string &name)
{
	errno = 0;
	mode_t mode = S_IRWXU;
	return ((mkdir(name.c_str(), mode) == 0) || (errno == EEXIST));
}

