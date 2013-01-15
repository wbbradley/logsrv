#pragma once
#include <string>
#include <stdint.h>
#include <iostream>
#include <tr1/memory>
#include <iosfwd>
#include <vector>
#include "logger_decls.h"
#include <dirent.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

bool ensure_directory_exists(const std::string &name);

