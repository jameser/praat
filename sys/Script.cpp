/* Script.cpp
 *
 * Copyright (C) 1997-2011 Paul Boersma
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * pb 2000/09/21
 * pb 2002/03/07 GPL
 * pb 2009/01/26
 * pb 2011/05/15 C++
 * pb 2011/07/04 C++
 */

#include "Script.h"

class_methods (Script, Data) {
	class_methods_end
}

Script Script_createFromFile (MelderFile file) {
	autoScript me = Thing_new (Script);
	MelderFile_copy (file, & my file);
	return me.transfer();
}

/* End of file Script.cpp */
