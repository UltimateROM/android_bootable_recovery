/*
	Copyright 2012-2016 bigbiff/Dees_Troy TeamWin
	This file is part of TWRP/TeamWin Recovery Project.

	TWRP is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	TWRP is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with TWRP.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <cctype>
#include "fixContexts.hpp"
#include "twrp-functions.hpp"
#include "twcommon.h"

using namespace std;


int fixContexts::restorecon(string entry, struct stat *sb) {
	return 0;
}

int fixContexts::fixContextsRecursively(string name, int level) {
	return 0;
}

int fixContexts::fixDataMediaContexts(string Mount_Point) {
	return 0;
}
