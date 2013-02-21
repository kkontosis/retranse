/*  libretranse, a regular expression based programming language library 
 *  Copyright (C) 2013, Kimon Kontosis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  version 2.1 along with this program (see LICENSE.LESSER); if not, see 
 *  <http://www.gnu.org/licenses/>.
 *
*/

#ifndef DIRLIST_INCLUDED
#define DIRLIST_INCLUDED

#include <vector>
#include <string>
#define __POSIX__
#ifdef __POSIX__
#include <dirent.h>
#endif
namespace retranse {

#ifdef __POSIX__
std::vector<std::string> dirlist(std::string dn)
{
	std::vector<std::string> r;
	DIR *d = opendir(dn.c_str());
	struct dirent* e;
	while(e = readdir(d)) {
		r.push_back(e->d_name);
	}
	closedir(d);
	return r;
}
#endif

} //namespace retranse

#endif //INCLUDE GUARD
