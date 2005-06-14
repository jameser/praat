/* Function.c
 *
 * Copyright (C) 1992-2004 Paul Boersma
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
 * pb 2002/07/16 GPL
 * pb 2003/07/10 NUMbessel_i0_f
 */

#include "Function.h"

#include "oo_COPY.h"
#include "Function_def.h"
#include "oo_EQUAL.h"
#include "Function_def.h"
#include "oo_WRITE_ASCII.h"
#include "Function_def.h"
#include "oo_READ_ASCII.h"
#include "Function_def.h"
#include "oo_WRITE_BINARY.h"
#include "Function_def.h"
#include "oo_READ_BINARY.h"
#include "Function_def.h"
#include "oo_DESCRIPTION.h"
#include "Function_def.h"

static double getXmin (I) { iam (Function); return my xmin; }
static double getXmax (I) { iam (Function); return my xmax; }

class_methods (Function, Data)
	class_method_local (Function, copy)
	class_method_local (Function, equal)
	class_method_local (Function, writeAscii)
	class_method_local (Function, readAscii)
	class_method_local (Function, writeBinary)
	class_method_local (Function, readBinary)
	class_method_local (Function, description)
	class_method (getXmin)
	class_method (getXmax)
class_methods_end

int Function_init (I, double xmin, double xmax) {
	iam (Function);
	my xmin = xmin;
	my xmax = xmax;
	return 1;
}

double Function_window (double tim, int windowType) {
	static double one_by_bessi_0_12, one_by_bessi_0_20;
	switch (windowType) {
		case Function_RECTANGULAR:
			if (tim < -0.5 || tim > 0.5) return 0.0;
			return 1;
		case Function_TRIANGULAR:
			if (tim < -0.5 || tim > 0.5) return 0.0;
			return 1 - tim - tim;
		case Function_PARABOLIC:
			if (tim < -0.5 || tim > 0.5) return 0.0;
			return 1 - 4 * tim * tim;
		case Function_HANNING:
			if (tim < -0.5 || tim > 0.5) return 0.0;
			return 0.5 + 0.5 * cos (2 * NUMpi * tim);
		case Function_HAMMING:
			if (tim < -0.5 || tim > 0.5) return 0.0;
			return 0.54 + 0.46 * cos (2 * NUMpi * tim);
		case Function_POTTER:
			if (tim < -0.77 || tim > 0.77) return 0.0;
			return 0.54 + 0.46 * cos (2 * NUMpi * tim);
		case Function_KAISER12:
			if (tim < -0.77 || tim > 0.77) return 0.0;
			if (! one_by_bessi_0_12) one_by_bessi_0_12 = 1.0 / NUMbessel_i0_f (12);
			return NUMbessel_i0_f (12 * sqrt (1 - (1.0 / 0.77 / 0.77) * tim * tim)) * one_by_bessi_0_12;
		case Function_KAISER20:
			if (tim <= -1 || tim >= 1) return 0.0;
			if (! one_by_bessi_0_20) one_by_bessi_0_20 = 1.0 / NUMbessel_i0_f (20.24);
			return NUMbessel_i0_f (20.24 * sqrt (1 - tim * tim)) * one_by_bessi_0_20;
		case Function_GAUSSIAN:
			return exp ((- NUMpi * NUMpi) * tim * tim);
		default:
			return 0.0;
	}
}

void Function_unidirectionalAutowindow (I, double *xmin, double *xmax) {
	iam (Function);
	if (*xmin >= *xmax) {
		*xmin = my xmin;
		*xmax = my xmax;
	}
}

void Function_bidirectionalAutowindow (I, double *x1, double *x2) {
	iam (Function);
	if (*x1 == *x2) {
		*x1 = my xmin;
		*x2 = my xmax;
	}
}

int Function_intersectRangeWithDomain (I, double *x1, double *x2) {
	iam (Function);
	if (*x1 == *x2) return 0;
	if (*x1 < *x2) {
		if (*x1 < my xmin) *x1 = my xmin;   /* Intersect requested range with logical domain. */
		if (*x2 > my xmax) *x2 = my xmax;
		if (*x2 <= *x1) return 0;   /* Requested range and logical domain do not intersect. */
	} else {
		if (*x2 < my xmin) *x2 = my xmin;   /* Intersect requested range with logical domain. */
		if (*x1 > my xmax) *x1 = my xmax;
		if (*x1 <= *x2) return 0;   /* Requested range and logical domain do not intersect. */
	}
	return 1;
}

/* End of file Function.c */