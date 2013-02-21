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

#ifndef RETRANSE_HPP_INCLUDED
#define RETRANSE_HPP_INCLUDED

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>
#include <exception>

namespace retranse {

struct node;

////////////////////////////////////////////////////////////////

//! Compile a program from an input stream, 0 on error. Delete node when done.
node* compile(std::istream& ii, std::string shown_filename, std::ostream& err);

//! Compile a program from a filename. Set quiet to not show errors.
inline node* compile(std::string f, bool quiet = false) {
	std::ifstream ii(f.c_str());
	if(quiet) {
		std::ostringstream s;
		return compile(ii, f, s);
	} else
	return compile(ii, f, std::cerr);
}

//! Compile a program from a string using the program name 'n'
inline node* compile(std::string prg, std::string n, bool quiet = false) {
	std::istringstream ii(prg);
	if(quiet) {
		std::ostringstream s;
		return compile(ii, n, s);
	} else
	return compile(ii, n, std::cerr);
}
////////////////////////////////////////////////////////////////

//! Show the compilation tree of n to std::cout
void recshow(node* n, int d);

////////////////////////////////////////////////////////////////

//! Variable space. Map that holds: map["varname"] = "value"
typedef std::map<std::string, std::string> var_t;

//! Exception for runtime errors
struct rtex : public std::exception {
	std::string s;
	rtex(std::string s) : s(s) {}
	rtex(const rtex& o) : s(o.s) {}
	rtex() : s() {}
	~rtex() throw() {}
	const char* what() const throw() { return s.c_str(); }
	template<class T>
	rtex operator <<(T x) const 
	{
		std::ostringstream st;
		st << s << x;
		return rtex(st.str());
	}
};

////////////////////////////////////////////////////////////////

//! Run program n function f with arguments 'ar' and variable space 'v'
std::string run(node* n, std::string f, std::vector<std::string> ar, var_t& v);

//! Run program n function f with single argument ar and variable space 'v' 
inline std::string run(node* n, std::string f, std::string ar, var_t& v) {
	return run(n, f, std::vector<std::string>(1, ar), v);
}
//! Run program n function f with arguments 'ar' 
inline std::string run(node* n, std::string f, std::vector<std::string> ar) {
	var_t v;
	return run(n, f, ar, v);
}
//! Run program n function f with single argument ar
inline std::string run(node* n, std::string f, std::string ar) {
	var_t v;
	return run(n, f, std::vector<std::string>(1, ar), v);
}
//! Run program n main with argument 's' and variable space 'v'
inline std::string run(node* n, std::string s, var_t& v) {
	return run(n, "main", std::vector<std::string>(1, s), v);
}
//! Run program n main with single argument ar
inline std::string run(node* n, std::string s) {
	var_t v;
	return run(n, "main", std::vector<std::string>(1, s), v);
}

} //namespace retranse

#endif //INCLUDE GUARD

