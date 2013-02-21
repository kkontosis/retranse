/*  retranse, a regular expression based programming language interpreter 
 *  Copyright (C) 2013, Kimon Kontosis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation; either version 2.0, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  version 2.0 along with this program (see LICENSE); if not, see 
 *  <http://www.gnu.org/licenses/>.
 *
*/

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include "retranse.hpp"


int help()
{
	std::cout << "retranse version 1.0\n";
	std::cout << "Copyright (c) 2013 Kimon Kontosis, licensed GPL v2.0 or above\n";
	std::cout << "Usage: retranse switch [sourcefile] [inputfile | arg] [outputfile]\n";
 	std::cout << "\n";
	std::cout << "-t		display compilation tree\n";
	std::cout << "-i		run source file and evaluate standard input\n";
	std::cout << "-f		run source file and evaluate file\n";
	std::cout << "-p		run source file and evaluate command line input\n";
	std::cout << "--help		display this help\n";
	std::cout << "\n";
	return 0;
}

int main(int argc, char** argv)
{
	if(argc <= 1 || argc > 1 && !strcmp(argv[1], "--help")) return help();

	std::istream* is = &std::cin;
	std::istream* ii = &std::cin;
	std::ostream* out = &std::cout;
	int mode = 0;
	int idn = 4;

	if(!strcmp(argv[1], "-t")) { mode='t'; is = &std::cin; }
	else if(!strcmp(argv[1], "-i")) { mode='r'; idn = 3; }
	else if(!strcmp(argv[1], "-f")) { 
		mode='r';
		if(argc <= 3) { std::cerr << "file not specified\n"; return 1; }
		ii = new std::ifstream(argv[3]);
		if(!ii->good()) { std::cerr << "cannot open input file\n"; return 1; }
	}
	else if(!strcmp(argv[1], "-p")) {
		if(argc <= 2) { std::cerr << "file not specified\n"; return 1; }
		if(argc <= 3) { std::cerr << "input argument not specified\n"; return 1; }
		mode='r';
		ii = new std::istringstream(argv[3]);
	}
	else { std::cerr << "invalid switch" << std::endl; return 1; }
	
	if(argc >= 3)
	is = new std::ifstream(argv[2]);
	if(!is->good()) { std::cerr << "cannot open source file\n"; return 1; }

	if(argc > idn)
	out = new std::ofstream(argv[idn]);
	if(!out->good()) { std::cerr << "cannot open output file\n"; return 1; }
	
	std::string srcname = (argc >= 3)? argv[2] : "stdin";

        retranse::node *n;
	n=retranse::compile(*is, srcname, std::cerr);
        if(!n) { std::cerr << "compile failed!\n"; return 1; }

	if(mode == 't') {
	        std::cout << srcname << " compiled.\n";
		retranse::recshow(n, 0);
		return 0;
	}

	std::string s;

	retranse::var_t v;

	while(ii->good()) {
		std::getline(*ii, s="");
		//(*ii) >> s;
		std::vector<std::string> vs(1, s);
		try {
			(*out) << retranse::run(n, "main", vs, v) << std::endl;
		} catch(retranse::rtex x) {
			std::cerr << "error = " << x.s << std::endl;
		}
	}
	return 0;
}

