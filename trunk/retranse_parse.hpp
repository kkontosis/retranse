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

#ifndef RETRANSE_PARSE_HPP_INCLUDED
#define RETRANSE_PARSE_HPP_INCLUDED

namespace retranse
{
node* compile(std::istream& ii, std::string shown_filename, std::ostream& err);
void recshow(node* n, int d);



}

#endif //INCLUDE GUARD

