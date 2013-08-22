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
#include <algorithm>
#include "retranse_node.hpp"
#include "dirlist.hpp"
#include <pcre++.h> // for include

#define L std::cout << 
static const unsigned char BOM[] = { 0xEF,0xBB,0xBF };

namespace retranse {
void preview(std::string l, std::istream& i)
{
	return;
	std::string s;
	i >> s;
	std::cout << "preview: (" << l << "):" << s << ":" << std::endl;
	i.clear();
	for(int j=0;j<s.size();j++) i.unget();
}

inline bool check_eof(std::istream& i)
{
	int c=i.get();
	if(c<0) i.clear(); else i.unget();
	return c<0;
}

// surrounds a node and some usefulness about reading data to it.
struct noder {
	node* n;
	std::string file;
	int line;
	noder(node* n, std::string file, int line) 
		: n(n), file(file), line(line) {}
	noder(const noder& o)
		: n(o.n), file(o.file), line(o.line) { }
	noder(const noder& o, node* n)
		: n(n), file(o.file), line(o.line) { }
};

// holds lines of code given from parser. 
struct noderholder {
	std::vector<noder*> ls;
	node* root;
	std::vector<node*> cur_f;
	std::map<std::string, std::string> meta;
	int curlabel;
	std::map<std::string, int> labels;
	std::ostream& err;
	std::istringstream* isp;
	std::string empt;
	int current;
	noderholder(std::string s, noderholder& h) : err(h.err), 
		ls(1, new noder(new node(s), "",0)), current(0), root(h.root), cur_f(h.cur_f),
			isp(new std::istringstream(s)) {}
	noderholder(std::ostream& err) : err(err), current(-1) { root=NULL;isp=NULL; }
	~noderholder() { for(int i=0;i<ls.size();i++) 
		{ // TODO: re-enable and fix: if(isp) delete isp; 
		if(ls[i]->n) delete ls[i]->n; delete ls[i]; } } // delete isp crashes
	noderholder& operator<<(const noder& n) { 
		ls.push_back(new noder(n, NULL)); current++; return *this; }
	void operator&&(const std::string& s) { ls[ls.size()-1]->n = new node(s); }
	template<class T>
	std::ostream& operator<<(const T& x);
	noder& operator()() { return *(ls[current]); }
	std::string& str() { return current >= ls.size() ? empt : ls[current]->n->str[0]; }
	void rewind() { current = 0; isp = new std::istringstream(str()); }
	bool eof() { return check_eof(*isp) && current+1 >= ls.size(); }
	std::istream& iss() { return *isp; }
	bool advance() { if(current >= ls.size()-1) return false; if(isp) delete isp; 
		current++; isp = new std::istringstream(str()); return true; }
};

// show compiler error
template<class T>
std::ostream& noderholder::operator<<(const T& x)
{
	err << ls.size() << std::endl;
	return err << ls[current]->file << ":" << ls[current]->line << ": " << x; 
}

// section 1 : plain string manip.
static const char* whitespace_chars=" \t";

// read all leading whitespace from stream.
int whitespace(std::istream& ii)
{
	int cnt=-1, c;
	do {
		c = ii.get();
		cnt++;
	} while(strchr(whitespace_chars, c));
	if(c<0) ii.clear(); else ii.unget();
	return cnt;
}

// true if string 'str' is waiting to be read from istream 'ii'.
bool check_str(std::istream& ii, std::string s, bool whspace = false, 
	const char* wh = whitespace_chars)
{
	int i;
	for(i=0;i<s.size();i++) {
		int c = ii.get();
		if(c < 0) { ii.clear(); for(;i>0;i--) ii.unget(); return false; }
		if(s[i] != c) break;
	}
	bool checkd = (i == s.size());
	if(checkd) i--; else whspace=false;
	if(whspace && check_eof(ii)) whspace=false;
	if(whspace) i++,checkd=strchr(wh, ii.get());
	for(;i>=0;i--) ii.unget();
	return checkd;
}

// true if string 'str' is read from istream 'ii'.
bool check_rd_str(std::istream& ii, std::string s, bool whspace = false,
	const char* wh = whitespace_chars)
{
	if(check_str(ii, s, whspace, wh)) { 
		for(int i=0; i<s.size(); i++) ii.get();
		return true;
	}
	return false;
}

// return true if end of line is reached, when reading preparsed string streams
bool eolchar(std::istream& ii)
{		
	return (check_eof(ii) || check_str(ii, ";") || check_str(ii, "}"));
}

// checks if any of the characters in 'chars'
// is waiting to be read from istream 'ii'.
bool check_chr(std::istream& ii, std::string chars)
{
	int c = ii.get();
	if(c<0) ii.clear(); else ii.unget();
	return (c>=0) && strchr(chars.c_str(), c);
}

// section 2: string manip with error reporting

// asserts that string is read from ii
void assert(std::istream& ii, std::string s, noderholder& nrd)
{
	if(!check_rd_str(ii, s)) {
		nrd << "error: Expected " << s << std::endl;
		throw(1);
	}
}

// fails if string is read from ii
void unexpect(std::istream& ii, std::string s, noderholder& nrd)
{
	if(check_str(ii, s)) {
		nrd << "error: Unexpected " << s << std::endl;
		throw(1);
	}
}

// section 3: compile item manip

// read a string without blanks or certain special characters for id
std::string scan_id_string(std::istream& ii, const char* additional_terminators = NULL)
{
	std::string s;
	while(!check_eof(ii)) {
		int c = ii.get();
		if(strchr("$(){} \t", c)) { ii.unget(); break; }
		if(additional_terminators && strchr(additional_terminators, c)) { ii.unget(); break; }
		s.push_back(c);
	}
	return s;
}

// read a string without whitespaces but allow escaped whitespaces.
// should also satisfy unescaped parentheses if track_par is true.
// counts and returns # of opened or closed parentheses if par_map is not null.
std::string scan_string(std::istream& ii, int track_par = false, 
	std::map<int, int>* par_map = NULL, bool test_par = false)
{
	const char* openpar = "([\"";
	const char* closepar = ")]\"";
	const char* cp;
	std::string s;
	int esc = 0;
	std::map<int, int> open;
	while(!check_eof(ii)) {
		int c = ii.get();
		if(!esc && strchr(" \t", c) &&!open['\"']) { ii.unget(); break; }
		if(esc) {
			s.push_back('\\');
		}
		else if(track_par || par_map || test_par) {
			if((cp=strchr(closepar, c)) && (c!='\"' || open[c])) {
				if(--open[*(cp-closepar+openpar)] < 0 
				&& track_par) {
					++open[*(cp-closepar+openpar)];
					ii.unget();
					break;
				}
			} else
			if(strchr(openpar, c) && (c!='\"' || !s.size())) open[c]++;
		}
		if(!esc && c=='\\') esc=1;
		else { if(c!='\"'||esc) s.push_back(c); esc=0; }
	}
	if(par_map) {
		for(std::map<int, int>::const_iterator it=open.begin(); it!=open.end(); it++)
			if(it->second) (*par_map)[it->first]=it->second;
	}
	return s;
}

// convert an escaped string to a normal one and mark escaped dollar-signs
std::vector<int> unesc_string(std::string& s)
{
	std::vector<int> mr(s.size(), 0);
	for(size_t i=0;i<s.size();i++)
		if(s[i]=='\\' && i<s.size()-1) {
			mr[i]=1;
			s.erase(i,1);
			if(s[i]=='e') { mr[i]=0; s.erase(i,1); i--; }
			else if(s[i]=='s') s[i]=' ';
			else if(s[i]=='n') s[i]='\n';
			else if(s[i]=='r') s[i]='\r';
			else if(s[i]=='t') s[i]='\t';
		}
	return mr;
}

// remove all $i and $(name) occurences from s and return list of reverse position and variable name.
std::vector<std::pair<int, std::string> > find_replacements(std::string& s, 
	std::vector<int>* escaped_map = NULL)
{
	int cp=0, d=0;
	std::pair<int, std::string> v;
	std::vector<std::pair<int, std::string> > r;
	for(int i=0; i+1<s.size(); i++) if(s[i]=='$' && strchr("(0123456789", s[i+1]))
	{
		if(escaped_map && escaped_map[0][i+d]) continue;
		v.first = i;
		if(s[i+1]=='(') {
			for(cp=i+1;cp<s.size()&&s[cp]!=')';cp++);
			if(cp>=s.size()) break;
			v.second=s.substr(i+2, cp-i-2);
			s.erase(i, cp-i+1);
			d+=cp-i+1;
		} else {
			v.second=s.substr(i+1, 1);
			s.erase(i, 2);
			d+=2;
		}
		i--;
		r.push_back(v);
	}
	std::reverse(r.begin(), r.end());
	return r;
}

// section 4: compile functions

// parse the {
bool parse_open_brace(noderholder& h)
{
	whitespace(h.iss());
	if(check_eof(h.iss()) && !h.advance()) {
		return false;
	}
	preview("closing", h.iss());
	whitespace(h.iss());
	bool rv = (check_rd_str(h.iss(), "{"));
	whitespace(h.iss());
	if(check_eof(h.iss()) && !h.advance()) {
		return false;
	}
	whitespace(h.iss());
	return rv;
}

// forward declaration required for this!
node* parse_body(noderholder& h, int m_flag);
node* parse_cmd(noderholder& h);

// parse the function header
node* parse_function(noderholder& h, int c_flag = 'f')
{
	node* n = new node;
	std::string cx;
	bool flg = true;
	if(c_flag=='f')
		assert(h.iss(), "function", h);
	else {
		assert(h.iss(), "call", h);
		whitespace(h.iss());
		if(c_flag == 'x') {
			cx = scan_id_string(h.iss(),"="); // var
			whitespace(h.iss());
			if(!check_rd_str(h.iss(), "=")) {
				n->str.push_back(cx); cx="1"; flg=false;
			}
		}
	}
	n->code.push_back(c_flag == 'r' ? 'x' : c_flag);

	whitespace(h.iss());
	if(flg) n->str.push_back(scan_id_string(h.iss()));
	whitespace(h.iss());
	assert(h.iss(), "(", h);
	do {
		//whitespace(h.iss());
		//if(n->str.size() > 1) assert(h.iss(), ",", h);
		whitespace(h.iss());
		n->str.push_back(scan_string(h.iss(), true));
		whitespace(h.iss());
	} while(!check_eof(h.iss()) && !check_str(h.iss(), ")"));
	assert(h.iss(), ")", h);
	if(c_flag!='f') {
		// number of arguments first
		n->code.push_back(n->str.size());
		// replacements for call
		int stn = n->str.size();
		for(int i=1; i<stn; i++) {
			std::vector<std::pair<int, std::string> > rpl
				= find_replacements(n->str[i]);
			n->code.push_back(rpl.size());
			for(int j=0; j<rpl.size(); j++) {
				n->code.push_back(rpl[j].first); // position in string
				n->code.push_back(n->str.size()); // variable name index in strs
				n->str.push_back(rpl[j].second);
			}
		}

		// code for call and store / return 
		std::ostringstream rcmd;
		if(c_flag == 'r') 
			rcmd << "cond ( \\e == \\e ) { reduce return }";
		else
			rcmd << "cond ( \\e == \\e ) { set "<<cx << " = $1 }";

		noderholder nrh(rcmd.str(), h);
		node* p = parse_cmd(nrh);
		p->flow[0]->flow.insert(p->flow[0]->flow.begin(), n);
		return p;
	}
	if(!parse_open_brace(h)) {
		h << "error: Expected function body" << std::endl;
		throw(1);
	}
	h.cur_f.push_back(n);
	n->flow.push_back(new node);
	n->flow[0]->code.push_back('l'); // local vars
	n->flow.push_back(parse_body(h, 'f'));
	h.cur_f.pop_back();

	return n;
}

// parse cond command
node* parse_cond(noderholder& h)
{
	node* n = new node;
	n->code.push_back('c');
	assert(h.iss(), "cond", h);
	whitespace(h.iss());
	assert(h.iss(), "(", h);
	
	std::vector<int> user_stack;
	std::vector<int> op_stack;
	std::vector<std::string> cmd_stack;
	int d = 0;
	int opc = 0;
	std::map<int, int> opi, ops;
	int idx = 0;

	while(!eolchar(h.iss())) {
		whitespace(h.iss());
		if(check_rd_str(h.iss(), "(", true)) { d++; opc=0; continue; }
		if(check_rd_str(h.iss(), ")", true)) {
			for(;ops[d];ops[d]--) {
				// one or two operands and one operator.
				n->code.push_back(*op_stack.rbegin());
				op_stack.pop_back(); 
				for(;opc>0;opc--) {
					n->str.push_back(*cmd_stack.rbegin());
					cmd_stack.pop_back();
				}
				for(;opi[d]>0;opi[d]--) {
					n->code.push_back(*user_stack.rbegin());
					user_stack.pop_back();
				}
				user_stack.push_back(idx++);
				opi[d]=1;
			}
			opi[d]=0;
			d--;
			opi[d]++;
			if(d<0) { h.iss().unget(); break; } else continue;
		}
		
		std::string wh_s = std::string(whitespace_chars) + "()";
		const char* wh = wh_s.c_str();

		// unary subexpression (flag 256) operator ! (may not require parenthesis)
		if(opc==0 && opi[d] == 0 && check_str(h.iss(), "!", true, wh))
				{ ops[d]++; op_stack.push_back(h.iss().get()|256); continue; }
		
		// binary operator ~ == != and || &&
		if((opc==1 && opi[d] == 0 && (
			check_str(h.iss(), "~", true) ||
			check_str(h.iss(), "==", true) ||
			check_str(h.iss(), "!=", true) 
		   	)) || (opc==0 && opi[d] == 1 && (
			check_str(h.iss(), "||", true) ||
			check_str(h.iss(), "&&", true))))
				{ ops[d]++; op_stack.push_back(scan_id_string(h.iss())[0]); continue; }

		// no-case flag on operator.
		if((opc==2) && ops[d] && check_rd_str(h.iss(), "[NC]", true))
			{ *op_stack.rbegin() |= 512; continue; }

		if(opc>=2) {
			h << "error: Expected )" << std::endl;
			throw(1);
		}
		if(opc>=1 && ops[d]==0) {
			std::string tmp; h.iss()>>tmp;
			h << "error: Expected operator" << tmp << ":" << opi[d] << "&" << ops[d] << "#" << d << std::endl;
			throw(1);
		}
		// parse string or regex
		opc++;
		std::map<int, int> pars;
		cmd_stack.push_back(scan_string(h.iss(), opc==2, opc==2?&pars:NULL));
		if(pars.size() && opc==2 && (*op_stack.rbegin()&0xff)=='~')
			h << "warning: Bad parentheses balance in regular expression" << std::endl;
	}

	// stage two: replacements
	int cdxn = n->code.size();
	int stn = n->str.size();
	for(int i=0; i<stn; i++) {
		std::vector<int> mri = unesc_string(n->str[i]);
		std::vector<std::pair<int, std::string> > rpl=find_replacements(n->str[i], &mri);
		n->code.push_back(rpl.size());
		for(int j=0; j<rpl.size(); j++) {
			n->code.push_back(rpl[j].first); // position in string
			n->code.push_back(n->str.size()); // variable name index in strs
			n->str.push_back(rpl[j].second);
		}
	}
	n->code.push_back(cdxn); // add index of replacements table at the end of op

	whitespace(h.iss());
	assert(h.iss(), ")", h);
	whitespace(h.iss());

	if(!parse_open_brace(h)) {
		if(h.eof()) {
			h << "error: Expected condition body" << std::endl;
			throw(1);
		}
		// one-liner
		n->flow.push_back(parse_body(h, '1'));
	} else
		n->flow.push_back(parse_body(h, 0));

	return n;
}

template<class T>
inline
bool vec_ifn_exist(const std::vector<T>& v, const T& s)
{
	return std::find(v.begin(), v.end(), s) == v.end();
}
template<class T>
inline
void vec_add_ifn_exist(std::vector<T>& v, const T& s)
{
	if(std::find(v.begin(), v.end(), s) == v.end())
		v.push_back(s);
}

bool test_glob_var(const std::string& s)
{
	if(s == "0") return true;
	if(s == "00") return false;
	if(s == "c") return false;
	if(s[0] == '0' || (s[0] >= '1' && s[0] <= '9') || strchr("ca",s[0])) {
		for(int i = 1; i < s.size(); i++)
			if(s[i] < '0' || s[i] > '9') return false;
		return true;
	}
	return false;
}

node* parse_set(noderholder& h, int g_flag = 0, int r_flag = 0)
{
	using namespace std;
	node* n = new node;
	n->code.push_back('s');
	n->code.push_back(g_flag);
	if(g_flag == 'g')
	assert(h.iss(), "global", h);
	else if(g_flag == 'r')
		{}//assert(h.iss(), "reduce", h);
	else assert(h.iss(), "set", h);
	whitespace(h.iss());
	if(g_flag=='r') {
		if(!r_flag) assert(h.iss(), "to", h);
		else assert(h.iss(), "return", h);
		n->str.push_back("");
	} else
		n->str.push_back(scan_id_string(h.iss(),"=")); // var
	if(n->str.back() == "-") n->str.back() = "";

	if(!g_flag) // add local variable if not exists
		if(vec_ifn_exist(h.root->flow[1]->str, *(n->str.rbegin()))) // not in globals
		if(!test_glob_var(*(n->str.rbegin()))) // not an always-global variable
			vec_add_ifn_exist((*h.cur_f.rbegin())->flow[0]->str, *(n->str.rbegin()));

	whitespace(h.iss());
	if(g_flag == 'g')
		vec_add_ifn_exist(h.root->flow[1]->str, *(n->str.rbegin())); // add to globals

	int rc = 1;
	size_t s0 = n->str.size();
	if(g_flag == 'g' && !check_str(h.iss(), "=")) 
		n->str.push_back(std::string("$("+(*n->str.rbegin())+")"));
	else {
		if(g_flag != 'r') assert(h.iss(), "=", h);
		whitespace(h.iss());
		rc=0;
		if(!r_flag) do {
			// implem. parsing of many, here!
			n->str.push_back(scan_string(h.iss(), false, NULL, true));
			whitespace(h.iss());
			rc++;
		} while(!eolchar(h.iss()));
	}
	if(rc == 1 && !n->str[0].size()) n->str[0]="1";
	n->code.push_back(rc);
	for(size_t i=0; i<rc; i++)
	{
		std::vector<int> mri = unesc_string((n->str[s0+i]));
		vector<pair<int, string> > rpl=find_replacements((n->str[s0+i]), &mri);
		n->code.push_back(rpl.size());
		for(int j=0; j<rpl.size(); j++) {
			n->code.push_back(rpl[j].first); // position in string
			n->code.push_back(n->str.size()); // variable name index in strs
			n->str.push_back(rpl[j].second);
		}
	}
	return n;	
}

node* parse_error(noderholder& h)
{
	using namespace std;
	node* n = new node;
	n->code.push_back('e');
	assert(h.iss(), "error", h);
	whitespace(h.iss());
	
	n->str.push_back(scan_string(h.iss(), false, NULL, true));
	std::vector<int> mri = unesc_string((n->str[0]));
	vector<pair<int, string> > rpl=find_replacements((n->str[0]), &mri);
	n->code.push_back(rpl.size());
	for(int j=0; j<rpl.size(); j++) {
		n->code.push_back(rpl[j].first); // position in string
		n->code.push_back(n->str.size()); // variable name index in strs
		n->str.push_back(rpl[j].second);
	}
	return n;	
}

node* parse_jump(noderholder& h)
{
	node* n = new node;
	n->code.push_back('j');
	whitespace(h.iss());
	std::string s = scan_id_string(h.iss());
	if(h.labels.find(s) == h.labels.end())
	{
		h << "error: Undefined label " << s << std::endl;
		throw(1);
	}
	n->code.push_back(h.labels[s]);
	return n;
}

node* parse_rex(noderholder& h)
{
	preview("rex",h.iss());
	node* n = new node;
	n->code.push_back('t');
	whitespace(h.iss());
	preview("rx1",h.iss());
	n->str.push_back(scan_string(h.iss(), false, NULL, true));
	whitespace(h.iss());
	preview("rx2",h.iss());
	if(check_eof(h.iss())) {
		h << "error: Replace statement requires two operands" << std::endl;
		throw(1);
	}
	if(check_rd_str(h.iss(), "-", true)) n->str.push_back("$0"); else
	n->str.push_back(scan_string(h.iss(), false, NULL, true));
	std::vector<int> mri = unesc_string(n->str[1]);
	whitespace(h.iss());
	int ncase=0;
	int lflag=0;
	int bflag=0;
	int lp=0;
	if(check_rd_str(h.iss(), "[")) {
	while(!eolchar(h.iss()) && !check_str(h.iss(), "]")) {
		whitespace(h.iss());
		if(check_rd_str(h.iss(), "NC")) ncase=1;
		else if(check_rd_str(h.iss(), "L")) lflag=1;
		else if(check_rd_str(h.iss(), "B")) bflag=1;
		else if(lp && check_rd_str(h.iss(), ",")) { lp=0; continue; }
		else {
			h << "error: Bad flag" << std::endl;
			throw(1);
		}
		lp=1;
	} assert(h.iss(), "]", h); }
	n->code.push_back(ncase);
	n->code.push_back(lflag);

	// stage two: replacements
	for(int i=0; i<2; i++) {
		std::vector<std::pair<int, std::string> > rpl
				= find_replacements(n->str[i], i==0?NULL:&mri);
		n->code.push_back(rpl.size());
		for(int j=0; j<rpl.size(); j++) {
			n->code.push_back(rpl[j].first); // position in string
			n->code.push_back(n->str.size()); // variable name index in strs
			n->str.push_back(rpl[j].second);
		}
	}

	// stage three: sub-program if any
	if(bflag) 
	{
		if(!parse_open_brace(h)) {
			h << "error: Expected statement body" << std::endl;
			throw(1);
		}
		n->flow.push_back(parse_body(h, 0));
		noderholder nr("reduce to $0", h);
		if(lflag) n->flow[0]->flow.push_back(parse_cmd(nr));
	} else if(lflag) {
		noderholder nr("reduce to $0", h);
		n->flow.push_back(parse_body(nr, '1'));
	}
	return n;
}

node* parse_cmd(noderholder& h)
{
	if(check_rd_str(h.iss(), "label", true)) {
		whitespace(h.iss());
		std::string lab=scan_id_string(h.iss());
		h.labels[lab]=h.curlabel;
		preview(std::string("lab")+lab,h.iss());
		whitespace(h.iss());
		assert(h.iss(), ":", h);
		whitespace(h.iss());
		}
	if(check_str(h.iss(), "cond", true, (std::string(whitespace_chars)+"(").c_str()))
		return parse_cond(h);
	if(check_str(h.iss(), "set", true))
		return parse_set(h);
	if(check_str(h.iss(), "error", true))
		return parse_error(h);
	if(check_str(h.iss(), "global", true))
		return parse_set(h, 'g');
	if(check_str(h.iss(), "call", true)) 
		return parse_function(h, 'x'); 
	if(check_rd_str(h.iss(), "reduce", true)) {
		whitespace(h.iss());
		if(check_str(h.iss(), "return"))
			return parse_set(h, 'r', 1);
		if(check_str(h.iss(), "to"))
			return parse_set(h, 'r');
		else if(check_str(h.iss(), "call"))
			return parse_function(h, 'r');
		else { 
			h << "error: Reduce expects \"to\" or \"call\" " << std::endl;
			throw(1);
		}
	}
	if(check_rd_str(h.iss(), "jump", true)) 
		return parse_jump(h);
	if(check_rd_str(h.iss(), "rerun", true)) {
		whitespace(h.iss());
		noderholder nhs("cond ( $0 != $(00) ) jump 0",h);
		nhs.labels["0"] = h.labels["0"];
		return parse_cond(nhs);
	}
	whitespace(h.iss());
	if(check_eof(h.iss()) || check_str(h.iss(), ";")) {
		h << "error: Empty command" << std::endl;
		throw(1);
	}
	return parse_rex(h);
}

node* parse_body(noderholder& h, int m_flag)
{
	node* n = new node;
	n->code.push_back('b');
	if(m_flag == 'm' || m_flag == 'f') {
		noderholder nrh("label 0 : set 00 = $0", h);
		h.curlabel = n->flow.size();
		n->flow.push_back(parse_cmd(nrh));
	}
	while(!h.eof())
	{
		h.curlabel = n->flow.size();
		whitespace(h.iss());
		if(check_str(h.iss(), "}")) break;

		if(check_str(h.iss(), "function", true))
		{
			if(m_flag != 'm') {
				h << "error: Function within function is not allowed." << std::endl;
				throw(1);
			}
			else
				h.root->flow[0]->flow.push_back(parse_function(h));
		} else
			n->flow.push_back(parse_cmd(h));

		whitespace(h.iss());

		if(!eolchar(h.iss())) {
			h << "error: Expected end of line." << std::endl;
			throw(1);
		}

		if(m_flag == '1') return n; // one-liner without braces		

		if(h.eof() || check_str(h.iss(), "}"))
			break;

		if(check_rd_str(h.iss(), ";")) whitespace(h.iss()); 
			//continue;

		if(check_eof(h.iss()) && !h.advance())
			break;

	} 

	if(m_flag == 'm' || m_flag == '1') unexpect(h.iss(), "}", h); 
	else assert(h.iss(), "}", h);
	if(m_flag == 'm' || m_flag == 'f') {
		noderholder nrh("reduce to $0", h);
		n->flow.push_back(parse_cmd(nrh));
	}
	return n;
}

node* parse_program(noderholder& h)
{
	using namespace std;
	node* root = new node;
	h.root = root;
	root->code.push_back('r'); // root
	root->flow.push_back(new node); 
	root->flow.push_back(new node);

	root->flow[1] = new node; // glob
	root->flow[1]->code.push_back('g');

	root->flow.push_back(new node);
	root->flow[2] = new node; // meta
	root->flow[2]->code.push_back('m');
	std::map<std::string, std::string>::const_iterator it;
	for(it=h.meta.begin();it!=h.meta.end();it++) {
		root->flow[2]->str.push_back(it->first);
		root->flow[2]->str.push_back(it->second);
	}

	root->flow[0] = new node; // funcs parent
	node* f = root->flow[0];
	f->code.push_back('p'); // funcs parent
	f->flow.push_back(new node);
	node* fm = f->flow[0]; // main function
	fm->code.push_back('f'); // function
	fm->str.push_back("main");
	fm->str.push_back("(.*)");
	h.cur_f.push_back(fm);
	fm->flow.push_back(new node);
	fm->flow[0]->code.push_back('l'); // local vars
	fm->flow.push_back(parse_body(h, 'm'));
	h.cur_f.pop_back();

	return root;
}

// section 5: preprocess functions

// preprocess the file, and parse into array of lines
void preprocess(std::istream& ii, noderholder& nrdh, noder nrd)
{
	using namespace std;
	string s, cs, ts;
	if(ii.good() && !check_eof(ii)) check_rd_str(ii, (const char*)BOM);
	while(ii.good() && !check_eof(ii)){
		getline(ii, s);
		//msdos text files
		if(s.size() && s[s.size()-1]=='\r') s.erase(s.size()-1);
		if(s.size() && s[0]=='\r') s.erase(0);
		if(s.size() && s[s.size()-1]=='\n') s.erase(s.size()-1);
		if(s.size() && s[s.size()-1]=='\r') s.erase(s.size()-1);
		nrd.line++;
		istringstream iss(s);
		whitespace(iss);
		if(check_rd_str(iss, "#include",true)) // include file
		{
			whitespace(iss);
			if(check_str(iss, "\"")) {
				assert(iss, "\"", nrdh);
				string fn;
				getline(iss, fn, '"');
				iss.unget();
				assert(iss, "\"", nrdh);
				ifstream ifs(fn.c_str(), ifstream::in);
				if(!ifs.good()) {
					nrdh << nrd << "error: Cannot include file " << fn << std::endl;
					throw(1);
				}
				noder n(NULL, fn, 0);
				preprocess(ifs, nrdh, n);
			} else {
				std::string rxs = scan_string(iss);
				const char* rc = rxs.c_str();
				const char* rd;
				std::string p = "";
				std::string rx = rxs;
				if(rd = strrchr(rc, '/')) {
					p = rxs.substr(0, rd-rc);
					rx = rxs.substr(rd-rc+1);
				}
				std::vector<std::string> dl = dirlist(p);
				if(p.size()) p += "/";
				for(size_t i = 0; i < dl.size(); i++) 
					if(pcrepp::Pcre(std::string("^")+rx+std::string("$"))
					.search(dl[i])) 
				{
					string fn = p + dl[i];
					ifstream ifs(fn.c_str(), ifstream::in);
					if(!ifs.good()) {
						nrdh << nrd << "error: Cannot include file " << fn << std::endl;
						throw(1);
					}
					noder n(NULL, fn, 0);
					preprocess(ifs, nrdh, n);
				}
			}
			continue;
		}
		if(check_rd_str(iss, "#meta",true)) // meta command
		{
			whitespace(iss);
			std::string key,val;
			key = scan_id_string(iss);
			whitespace(iss);
			val = scan_string(iss);
			nrdh.meta[key]=val;
			if(key == "retranse-dialect" && val != "A") {
				nrdh << nrd && s;
				nrdh.rewind();
				nrdh << "error: This version cannot parse any dialect other than A" << std::endl;
				throw(1);
			}
			continue;
		}
		if(check_str(iss, "#")) // ignore comment line
			continue;
		if(check_eof(iss)) // ignore empty line
			continue;
		while(!check_eof(iss)) {
			whitespace(iss);
			ts = scan_string(iss);
		} // check if last '\' right before eol is escaped or not
		if(s.size() && s[s.size()-1]=='\\' && 
			(!ts.size() || ts[ts.size()-1]!='\\'))
		// line that continues on the next line
		{
			cs += s;
			continue;
		}
		else
		{
			cs += s ; //+ "\n"; //" ;";
			(nrdh << nrd) && cs;
			cs.clear();
			continue;
		}
	}
}



node* compile(std::istream& ii, std::string shown_filename, std::ostream& err)
{
	noderholder nrdh(err);
	noder nrd(NULL, shown_filename, 0);
	try {
		preprocess(ii, nrdh, nrd);
		nrdh.rewind();
		return parse_program(nrdh);
	} catch(...)
	{
	return 0;
	}
}


void recshow(node* n, int d)
{
	for(int i=0;i<d;i++) std::cout <<"  ";
	std::cout << (char)n->code[0];
	for(int i=1;i<n->code.size(); i++) {
		std::cout << " " << n->code[i];
	}
	std::cout << "|";
	for(int i=0;i<n->str.size(); i++) {
		std::cout << n->str[i] << ":";
	}
	std::cout <<"\n";
	for(int i=0;i<n->flow.size(); i++) {
		recshow(n->flow[i], d+1);
	}
}
} //namespace retranse

#ifdef RETRANSE_PARSE_MAIN
int main()
{
	using namespace std;
	retranse::node *n;
	try {
	n=retranse::compile(std::cin, "stdin", std::cerr);
	}
	catch(...) { std::cerr << "Bailed out!\n"; return 0; }
	std::cout << "finished:\n";
	retranse::recshow(n, 0);
	delete n;
	return 0;
}
#endif

