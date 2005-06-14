#ifndef _Pitch_extensions_h_
#define _Pitch_extensions_h_
/* Pitch_extensions.h
 *
 * Copyright (C) 1993-2003 David Weenink
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 djmw 19970408
 djmw 20020813 GPL header
 djmw 20030220 Latest modification
*/

#ifndef _Pitch_h_
	#include "Pitch.h"
#endif

#ifndef _PitchTier_h_
	#include "PitchTier.h"
#endif

void Pitch_Frame_addPitch (Pitch_Frame me, double f, double strength, int maxnCandidates);
void Pitch_Frame_getPitch (Pitch_Frame me, double *f, double *strength);
void Pitch_Frame_resizeStrengths (Pitch_Frame me, double maxStrength,
	double unvoicedCriterium);
	
Pitch Pitch_scaleTime (Pitch me, double scaleFactor);
/*  Scale time domain and pitches:
	xmin' = xmin; dx' = dx * scaleFactor; x1' = xmin + 0.5 * dx'; 
	xmax' = xmin + nx * dx';
	pitch[i]' = pitch[i]/scaleFactor;
*/

void PitchTier_modifyRange (PitchTier me, double tmin, double tmax,
	double fmin, double factor, double fmid);
/*
	Scales the pitches in the interval (tmin, tmax) according to
	f = fmid + (f - fmid) * factor;
	f = f < fmin ? fmin : f;

*/
 
#endif /* _Pitch_extensions_h_ */