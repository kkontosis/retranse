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

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <pcre++.h>
#include "retranse_node.hpp"
#include "retranse_parse.hpp"
#include "retranse.hpp"

#define S0 std::cout << "\n"; std::cout.flush(); std::cout << 
std::ostringstream foo;
#if 1
#undef S0
#define S0 foo <<
#endif

namespace retranse {

using namespace pcrepp;

//struct retranse_player;

// node of execution stack
struct stacknode {
	std::vector<std::string> pv; // pushed variables
	std::vector<int> pvi; // pushed variables codes
	node* pc;
	int ofs;
	stacknode() : pc(0), ofs(0) { }
	stacknode(const stacknode& o) : pv(o.pv), pvi(o.pvi), pc(o.pc), ofs(o.ofs) { }
	stacknode(node* n) : pc(n), ofs(0) { }
};

typedef std::vector<stacknode> stack_t;


// section 0: intermediate functions on stack

stacknode call_function(std::string fn, std::vector<std::string> ar, node* root, var_t& v)
{
	//S0 fn << "[" << ar.size() << "#" << ar[0] << "|" << (ar.size()>1 ? ar[1] : "") << "]\n";

	node* ff = root->flow[0];
	node* n=0;
	std::vector<std::string> c;
	size_t i;
	for(i = 0; i < ff->flow.size(); i++) 
		if(ff->flow[i]->str[0] == fn) {
		n = ff->flow[i];
		if(ar.size() != n->str.size() - 1)
			continue;
		c.clear();
		size_t j;
		for(j = 0; j < ar.size(); j++) {
			Pcre t(std::string("^") + n->str[j + 1] + "$");
			if(!t.search(ar[j])) break;
			std::vector<std::string>* pc = t.get_sub_strings();
			if(pc) c.insert(c.end(), pc->begin(), pc->end());
			//replace(s, n, 1);
		}
		if(j != ar.size()) continue;
		if(!c.size()) c.insert(c.end(), ar.begin(), ar.end());
		break;
	}
	if(i == ff->flow.size())
		throw rtex("Cannot match function ") << fn;
	//n = n->flow[1]; // body
	stacknode sn(n);

	// push $0
	sn.pv.push_back(v["0"]);

	// set arguments to $0, $1, $2, ...
	// push and set arguments to $(a1), $(a2), ...
	std::string sx="0";
	sn.pvi.push_back(c.size());
	for(size_t j = 0; j<c.size(); j++) {
		std::ostringstream s;
		v[sx] = c[j];
		s << (j + 1);
		std::string sa = std::string("a") + s.str();
		sn.pv.push_back(v[sa]);
		v[sa] = v[sx];
		sx = s.str();
	}

	return sn;
}

stacknode enter_function(stacknode& c, var_t& v)
{
	node* lv = c.pc->flow[0]; // local vars

	// push locals to stack
	for(size_t i = 0; i < lv->str.size(); i++)
		c.pv.push_back(v[lv->str[i]]);

	node* n = c.pc->flow[1]; // body
	stacknode sn(n);
	return sn;
}

void leave_function(stacknode& c, var_t& v)
{
	node* lv = c.pc->flow[0]; // local vars

	// pop locals from stack
	for(int i = lv->str.size() - 1; i >= 0; i--) {
		v[lv->str[i]] = c.pv.back();
		c.pv.pop_back();
	}
	
	size_t ci = c.pvi.back();
	c.pvi.pop_back();

	// pop arguments $(a1), $(a2), ...
	for(int j = ci; j>= 1; j--) {
		std::ostringstream s;
		s << j;
		std::string sa = std::string("a") + s.str();
		v[sa] = c.pv.back();
		c.pv.pop_back();
	}

	// pop $0
	v["0"] = c.pv.back();
	c.pv.pop_back();

}

// section 1: execution tools

size_t replace(std::string& s, node& n, size_t o, var_t& v)
{
	size_t rsz = n.code[o];
	size_t p = o + 1;
	for(size_t i = 0; i < rsz; i++) {
		s.insert(n.code[p], v[n.str[n.code[p+1]]]);
		p += 2;
	}
	return p;
}

// section 2: actions

bool action_test(node& n, var_t& v, stacknode& z) 
{
	std::string x = n.str[0];
	std::string r = n.str[1];
	size_t p = replace(x, n, 3, v);

	size_t fl0 = 0;
	if(n.code[1]) fl0 |= PCRE_CASELESS;
	Pcre t(std::string("^") + x + "$", fl0);
	S0 "testing " << x << " for " << v["0"];
	if(!t.search(v["0"])) return false;
	S0 "passed";
	std::vector<std::string>* pc = t.get_sub_strings();
	// set matched arguments to $1, $2, $3, ...
	if(pc) for(size_t i = 0; i < pc->size(); i++) {
		std::ostringstream s;
		s << (i+1);
		v[s.str()] = pc[0][i];
	}
	replace(r, n, p, v);
	v["0"] = r;
	S0 "setting 0 to " << r;
	if(n.flow.size()) {
		z = stacknode(n.flow[0]);
		return true;
	}
	return false;
}

void action_set(node&n, var_t& v) 
{
	int rc = n.code[2];
	if(!rc) return;
	int p = 3;
	std::vector<std::string> s(rc);
	for(int i=1;i<=rc;i++) {
		s[i-1] = n.str[i];
		p = replace(s[i-1], n, p, v);
	}		
	if(rc > 1) for(int i=1;i<=rc+1;i++) {
		std::ostringstream sx;
		sx << n.str[0] << i;
		if(i==rc+1) v[sx.str()] = "";
		else 
		v[sx.str()] = s[i-1];
	}
	else {
		v[n.str[0]] = s[0];
		if(n.code[1] == 'r' && n.str[0] == "1") v["2"] = "";
	}

	// terminator
	std::ostringstream sx;
	sx << rc;
	v["01"] = sx.str();
}

void action_error(node&n, var_t& v) 
{
	S0 "act-error\n";	
	std::string s = n.str[0];
	replace(s, n, 1, v);
	throw rtex(s);
}

stacknode action_call(node* root, node&n, var_t& v) 
{
	S0 "act-call\n";
	std::vector<std::string> ar;
	size_t sz = n.code[1];
	ar.assign(n.str.begin() + 1, n.str.begin() + sz);
	size_t p = 2;
	for(size_t i = 1; i < sz; i++)
		p = replace(ar[i-1], n, p, v);
	return call_function(n.str[0], ar, root, v);
}

bool action_cond(node& n, var_t& v, stacknode& z) 
{
	// replacements
	std::vector<std::string> vs = n.str;
	size_t p = n.code.back();
	size_t i = 0;
	size_t cc = 1;
	while(p < n.code.size() - 1)
		p = replace(vs[i++], n, p, v);

	// ops
	p = n.code.back();
	size_t j = 0;
	size_t lp = 0;
	std::vector<int> res(p);
	for(i = 1; i < p; i++) {
		int op = n.code[i];
		int flg = op - (op & 0xff);
		op = op;
		lp = i-1;

		switch(op & 511) {
			case '~':
			S0 "~" << vs[j] << " " << vs[j+1] << "\n";
			{
			std::string rx = vs[j++];
			std::string sx = vs[j++];
			size_t fl0 = 0;
			if(flg & 512) fl0 |= PCRE_CASELESS;
			Pcre t(std::string("^") + rx + "$", fl0);
			bool rr = t.search(sx);
			std::vector<std::string>* pc = 0;
			if(rr) pc = t.get_sub_strings();
			// set matched arguments to $(c1), $(c2), $(c3), ...
			if(pc) for(size_t k = 0; k < pc->size(); k++) {
				std::ostringstream s;
				s << "c" << (cc++);
				v[s.str()] = pc[0][k];
			}
			res[lp] = rr;
			S0 "rrx " << rr << std::endl;
			}
			break;
			case '=':
			res[lp] = vs[j++] == vs[j++];
			break;
			case '!':
			res[lp] = vs[j++] != vs[j++];
			break;
			//
			case '|':
			res[lp] = res[n.code[++i]] || res[n.code[i+1]]; ++i;
			break;
			case '&':
			res[lp] = res[n.code[++i]] && res[n.code[i+1]]; ++i;
			break;
			//
			case '!'|256:
			res[lp] = !res[n.code[++i]];
			break;
			//
			default:
			throw rtex("Bad conditional op in binary code");			

		}


	}

	bool rrx = res[lp];
	S0 "cond tested res@" << p-2 << ":" << rrx;
	if(!rrx) return false;
	S0 "cond passed";	
	S0 "\n";
	if(n.flow.size()) {
		z = stacknode(n.flow[0]);
		return true;
	}
	return false;
}

// section 3: loops

void rloop(node* root, stacknode n0, var_t& v) // b-node
{
	std::vector<stacknode> i(1, n0);
	i.push_back(enter_function(i.back(), v));
	stacknode x;
	
	while(i.size()) {
		stacknode& p = i.back();
		S0 p.pc->flow.size() << " > " << p.ofs << "\n";
		node& n = p.pc->flow[p.ofs++][0];

		S0 (char)n.code[0];
		S0 "\n";

		switch(n.code[0]) {

		case 't': 
		if(action_test(n, v, x))
			i.push_back(x);
		break;
		case 'c':
		if(action_cond(n, v, x))
			i.push_back(x);
		break;
		case 'j':
		S0 "jump";
		while((i.rbegin()+1)->pc->code[0] != 'f') 
				i.pop_back();
		i.back().ofs = n.code[1];
		break;
		case 's':
		action_set(n, v);
		if(n.code[1]=='r') { // code for return
			S0 "ret\n";
			while(i.back().pc->code[0] != 'f') 
				i.pop_back(); 
			leave_function(i.back(), v); 
			i.pop_back();
		}
		break;
		case 'e':
		action_error(n, v);
		break;
		case 'x':
		i.push_back(action_call(root, n, v));
		i.push_back(enter_function(i.back(), v));
		break;

		default:
		throw rtex("Invalid command in binary code");	
		}
		// code to return from internal branches:
		while(i.size() && i.back().ofs >= i.back().pc->flow.size())
		{
			S0 "back from " << (char)i.back().pc->code[0];
			i.pop_back();
		}
	}

}

std::string run(node* n, std::string f, std::vector<std::string> ar, var_t& v)
{
	rloop(n, call_function(f, ar, n, v), v);
	return v["1"];
}

} //namespace retranse

#ifdef RETRANSE_RUN_MAIN
int main(int argc, char** argv)
{
        retranse::node *n;
        try {
	std::ifstream fi(argv[1]);
        n=retranse::compile(fi, argv[1], std::cerr);
        }
        catch(...) { std::cerr << "Bailed out!\n"; return 0; }
        std::cout << argv[1] << " compiled.\n";
        //retranse::recshow(n, 0);

	std::string s;

	retranse::var_t v;
	using namespace retranse;

	while(std::cin.good()) {
		std::cin >> s;
		std::vector<std::string> vs(1, s);
		try {
		rloop(n, call_function("main", vs, n, v), v);
		} catch(retranse::rtex x) {
		std::cerr << "error = " << x.s << std::endl;
		}
		std::cout << "result = " << v["1"] << std::endl;
	}

}
#endif

