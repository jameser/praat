/* NUM2.c
 *
 * Copyright (C) 1993-2011 David Weenink
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
 djmw 20020819 GPL header
 djmw 20020819 Split nonGLP part off.
 djmw 20001109 Changed stop criteria in NUMsvdcmp and NUMtqli.
 djmw 20020819 Split into GPL and nonGPL part.
 djmw 20021008 Removed SVD_sort.
 djmw 20030619 Removed calls to NRC svd-routines.
 djmw 20030623 Removed tqli en tred calls.
 djmw 20030703 Replaced NUMincompleteBeta with gsl_sf_beta_inc.
 djmw 20030710 NUMminimize_brent now also returns the minimum function value.
 djmw 20030731 NUMridders: better approximation for small d.
 			   NUMinvFisherQ better approximation for p < 0.5
 djmw 20030813 Added NUMmad and NUMstatistics_huber.
 djmw 20030825 Replaced gsl_sf_beta_inc with NUMincompleteBeta
 pb   20030828 Improvements for invFisherQ, ridders, studentP, studentQ,
 	invStudentQ, invChiSquareQ: modifications for 'undefined' return values.
 djmw 20030830 Corrected a bug in NUMtriangularfilter_amplitude
 djmw 20031111 Added NUMdmatrix_transpose, NUMdmatrix_printMatlabForm
 djmw 20040105 Added NUMmahalanobisDistance_chi
 djmw 20040211 Modified NUMstrings_copyElements: if (form[i]==NULL) then {to[i]= NULL}.
 djmw 20040303 Added NUMstring_containsPrintableCharacter.
 djmw 20050406 NUMprocrutus->NUMprocrustes
 djmw 20060319 NUMinverse_cholesky: calculation of determinant is made optional
 djmw 20060517 Added NUMregexp_compile
 djmw 20060518 Treat NULL string as empty string in strs_replace_regexp/literal. Don't accept empty search in str_replace_regexp
 djmw 20060626 Extra NULL argument for ExecRE.
 djmw 20061021 printf expects %ld for 'long int' for 64-bit systems
 djmw 20070302 NUMclipLineWithinRectangle
 djmw 20070614 updated to version 1.30 of regular expressions.
 djmw 20071022 Removed function NUMfvector_moment2.
 djmw 20071201 Melder_warning<n>
 djmw 20080107 Changed assertion to "npoints > 0" in NUMcosinesTable
 djmw 20080110 Corrected some bugs in str_replace_regexp
 djmw 20080122 Bug in str_replace_regexp
 djmw 20080317 +NUMsinc
 pb   20080410 FisherQ from gsl
 djmw 20090630 NUMlogNormalP/Q from gsl
 djmw 20090707 Rename NUMinverse_cholesky to NUMlowerCholeskyInverse,
 	+NUMcovarianceFromColumnCentredMatrix, +NUMmultivariateKurtosis
 djmw 20100311 +NUMsolveQuadraticEquation
 djmw 20100426 replace wcstok by Melder_wcstok
 djmw 20101209 removed NUMwcscmp is Melder_wcscmp now
 djmw 20110304 Thing_new
*/

#include <vector>
#include "Interpreter.h"
#include "SVD.h"
#include "Eigen.h"
#include "NUMclapack.h"
#ifndef _NUM_h_
	#include "NUM.h"
#endif
#include "NUM2.h"
#include "NUMmachar.h"
#include "melder.h"
#include <ctype.h>

#include "gsl_errno.h"
#include "gsl_sf_bessel.h"
#include "gsl_sf_gamma.h"
#include "gsl_sf_erf.h"
#include "gsl_sf_trig.h"
#include "gsl_poly.h"
#include "gsl_cdf.h"

#define my me ->

#undef MAX
#undef MIN

#define MAX(m,n) ((m) > (n) ? (m) : (n))
#define MIN(m,n) ((m) < (n) ? (m) : (n))
#define SIGN(a,b) ((b < 0) ? -fabs(a) : fabs(a))
using namespace std;
extern "C" machar_Table NUMfpp;

struct pdf1_struct { double p; double df; };
struct pdf2_struct { double p; double df1; double df2; };

int NUMdmatrix_hasInfinities (double **m, long rb, long re, long cb, long ce)
{
	double min = m[rb][cb];
	double max = min;
	for (long i = rb; i <= re; i++)
	{
		for (long j = cb; j <= ce; j++)
		{
			if (m[i][j] > max) max = m[i][j];
			else if (m[i][j] < min) min = m[i][j];
		}
	}
	if (! NUMfpp) NUMmachar ();
	return max >= NUMfpp -> rmax || min <= - NUMfpp -> rmax;
}

int NUMstring_containsPrintableCharacter (const wchar_t *s)
{
	long len;
	if (s == NULL || ((len = wcslen (s)) == 0)) return 0;
	for (long i = 0; i < len; i++)
	{
		int c = s[i];
		if (isgraph (c)) return 1;
	}
	return 0;
}

double *NUMstring_to_numbers (const wchar_t *s, long *numbers_found)
{
	*numbers_found = Melder_countTokens (s);
	if (*numbers_found < 1) Melder_throw ("Empty string.");
	autoNUMvector<double> numbers (1, *numbers_found);
	long inum = 1;
	for (wchar_t *token = Melder_firstToken (s); token != 0; token = Melder_nextToken (), inum++)
	{
		Interpreter_numericExpression (0, token, &numbers[inum]);
	}
	return numbers.transfer();
}
#if 0
int NUMstrings_equal (wchar_t **s1, wchar_t **s2, long lo, long hi)
{
	for (long i = lo; i <= hi; i++)
	{
		if (Melder_wcscmp (s1[i], s2[i])) return 0;
	}
	return 1;
}
#endif
void NUMstrings_copyElements (wchar_t **from, wchar_t**to, long lo, long hi)
{
	for (long i = lo; i <= hi; i++)
	{
		Melder_free (to[i]);
		if (from[i]) to[i] = Melder_wcsdup (from[i]); therror
	}
}

void NUMstrings_free (wchar_t **s, long lo, long hi)
{
	if (s == NULL) return;
	for (long i = lo; i <= hi; i++) Melder_free (s[i]);
	NUMvector_free<wchar_t *> (s, lo);
}

wchar_t **NUMstrings_copy (wchar_t **from, long lo, long hi)
{
	autoNUMvector<wchar_t *> to (lo, hi);
	NUMstrings_copyElements (from, to.peek(), lo, hi);
	return to.transfer();
}

static wchar_t *appendNumberToString (const wchar_t *s, long number)
{
	wchar_t buf[12];
	long ncharb, nchars = 0;

	ncharb = swprintf (buf, 12, L"%ld", number);
	if (s != NULL) nchars = wcslen (s);
	wchar_t *newc = Melder_calloc (wchar_t, nchars + ncharb + 1); 
	if (nchars > 0) wcsncpy (newc, s, nchars);
	wcsncpy (newc + nchars, buf, ncharb + 1);
	return newc;
}

int NUMstrings_setSequentialNumbering (wchar_t **s, long lo, long hi,
	const wchar_t *pre, long number, long increment)
{
	for (long i = lo; i <= hi; i++, number += increment)
	{
		wchar_t *newc = appendNumberToString (pre, number);
		if (newc == NULL) return 0;
		Melder_free (s[i]);
		s[i] = newc;
	}
	return 1;
}

#define HIGHBYTE(x) ((unsigned char) ((x) & 0xFF))
#define LOWBYTE(x)  ((unsigned char) ((x) >> 8 & 0xFF))

/* a+b=c in radix 256 */
void NUMstring_add (unsigned char *a, unsigned char *b, unsigned char *c, long n);
void NUMstring_add (unsigned char *a, unsigned char *b, unsigned char *c, long n)
{
	int j;
	unsigned short reg = 0;

	for (j = n; j > 1; j--)
	{
		reg = a[j] + b[j] + HIGHBYTE (reg);
		c[j+1] = LOWBYTE (reg);
	}
}

wchar_t *strstr_regexp (const wchar_t *string, const wchar_t *search_regexp)
{
	wchar_t *charp = 0;
	regularExp_CHAR *compileMsg;
	regexp *compiled_regexp = CompileRE ((regularExp_CHAR *) search_regexp, &compileMsg, 0);

	if (compiled_regexp == 0) Melder_throw ("No regexp");

	if (ExecRE(compiled_regexp, NULL, (regularExp_CHAR *) string, NULL, 0, '\0', '\0', NULL, NULL, NULL)) {
		charp = (wchar *) compiled_regexp -> startp[0];
	}

	free (compiled_regexp);
	return charp;
}

wchar_t *str_replace_literal (const wchar_t *string, const wchar_t *search, const wchar_t *replace,
	long maximumNumberOfReplaces, long *nmatches)
{
	if (string == 0 || search == 0 || replace == 0) return NULL;


	int len_string = wcslen (string);
	if (len_string == 0) maximumNumberOfReplaces = 1;
	int len_search = wcslen (search);
	if (len_search == 0) maximumNumberOfReplaces = 1;

	/*
		To allocate memory for 'result' only once, we have to know how many
		matches will occur.
	*/

	const wchar_t *pos = string; //current position / start of current match
	*nmatches = 0;
	if (maximumNumberOfReplaces <= 0) maximumNumberOfReplaces = LONG_MAX;

	if (len_search == 0) /* Search is empty string... */
	{
		if (len_string == 0) *nmatches = 1; /* ...only matches empty string */
	}
	else
	{
		if (len_string != 0) /* Because empty string always matches */
		{
			while ((pos = wcsstr (pos, search)) && *nmatches < maximumNumberOfReplaces)
			{
				pos += len_search;
				(*nmatches)++;
			}
		}
	}

	int len_replace = wcslen (replace);
	int len_result = len_string + *nmatches * (len_replace - len_search);
	wchar_t *result = Melder_malloc (wchar_t, (len_result + 1) * sizeof (wchar_t));
	result[len_result] = '\0';

	const wchar_t *posp = pos = string;
	int nchar = 0, result_nchar = 0;
	for (long i = 1; i <= *nmatches; i++)
	{
		pos = wcsstr (pos, search);

		/*
			Copy gap between end of previous match and start of current.
		*/

		nchar = (pos - posp);
		if (nchar > 0)
		{
			wcsncpy (result + result_nchar, posp, nchar);
			result_nchar += nchar;
		}

		/*
			Insert the replace string in result.
		*/

		wcsncpy (result + result_nchar, replace, len_replace);
		result_nchar += len_replace;

		/*
			Next search starts after the match.
		*/

		pos += len_search;
		posp = pos;
	}

	/*
		Copy gap between end of match and end of string.
	*/

	pos = string + len_string;
	nchar = pos - posp;
	if (nchar > 0) wcsncpy (result + result_nchar, posp, nchar);
	return result;
}

wchar_t *str_replace_regexp (const wchar_t *string, regexp *compiledSearchRE,
	const wchar_t *replaceRE, long maximumNumberOfReplaces, long *nmatches)
{
	int buf_nchar = 0;				/* # characters in 'buf' */
	int gap_copied = 0;
	int nchar, reverse = 0;
	int errorType;
	wchar_t prev_char = '\0';
	const wchar_t *pos; 	/* current position in 'string' / start of current match */
	const wchar_t *posp; /* end of previous match */
	autostring buf;

	*nmatches = 0;
	if (string == 0 || compiledSearchRE == 0 || replaceRE == 0) return 0;

	int string_length = wcslen (string);
	int replace_length = wcslen (replaceRE);
	if (string_length == 0) maximumNumberOfReplaces = 1;

	long i = maximumNumberOfReplaces > 0 ? 0 : - string_length;

	/*
		We do not know the size of the replaced string in advance,
		therefor, we allocate a replace buffer twice the size of the
		original string. After all replaces have taken place we do a
		final realloc to the then exactly known size.
		If during the replace, the size of the buffer happens to be too
		small (this is signalled by the replaceRE function),
		we double its size and restart the replace.
	*/

	int buf_size = MAX (2 * string_length, 100);
	buf.resize (buf_size);

	pos = posp = string;
	while (ExecRE(compiledSearchRE, 0, (regularExp_CHAR *) pos, 0, reverse, prev_char, '\0', 0, 0, 0) && i++ < maximumNumberOfReplaces)
	{
		/*
			Copy gap between the end of the previous match and the start
			of the current match.
			Check buffer overflow. pos == posp ? '\0' : pos[-1],
		*/

		pos = (wchar_t *) compiledSearchRE -> startp[0];
		nchar = pos - posp;
		if (nchar > 0 && ! gap_copied)
		{
			if (buf_nchar + nchar + 1 > buf_size)
			{
				buf_size *= 2;
				buf.resize (buf_size);
			}
			wcsncpy (buf.peek() + buf_nchar, posp, nchar);
			buf_nchar += nchar;
		}

		gap_copied = 1;

		/*
			Do the substitution. We can only check afterwards for buffer
			overflow. SubstituteRE puts null byte at last replaced position and signals when overflow.
		*/

		if ((SubstituteRE (compiledSearchRE, (regularExp_CHAR *) replaceRE, (regularExp_CHAR *) buf.peek() + buf_nchar, buf_size - buf_nchar, &errorType)) == false)
		{
			if (errorType == 1) // not enough memory
			{
				buf_size *= 2;
				buf.resize (buf_size);
				Melder_clearError ();
				i--; // retry
				continue;
			}
			Melder_throw ("Error during substitution.");
		}

		// Buffer is not full, get number of characters added;

		nchar = wcslen (buf.peek() + buf_nchar);
		buf_nchar += nchar;

		// Update next start position in search string.

		posp = pos;
		pos = (wchar_t *) compiledSearchRE -> endp[0];
		if (pos != posp) prev_char = pos[-1];
		gap_copied = 0;
		posp = pos; //pb 20080121
		(*nmatches)++;
		// at end of string?
		// we need this because .* matches at end of a string
		if (pos - string == string_length) break;
	}

	// Copy last part of string to destination string

	nchar = (string + string_length) - pos;
	buf_size = buf_nchar + nchar + 1;
	buf.resize (buf_size);

	wcsncpy (buf.peek() + buf_nchar, pos, nchar);
	buf[buf_size-1] = '\0';
	return buf.transfer();
}

static wchar_t **strs_replace_literal (wchar_t **from, long lo, long hi,
	const wchar_t *search, const wchar_t *replace, int maximumNumberOfReplaces,
	long *nmatches, long *nstringmatches)
{
	wchar_t ** result = NULL;
	try {
		long nmatches_sub = 0;

		if (search == NULL || replace == NULL) return NULL;
		autoNUMvector<wchar_t *> result (lo, hi);
		*nmatches = 0; *nstringmatches = 0;
		for (long i = lo; i <= hi; i++)
		{
			/* Treat a NULL as an empty string */
			const wchar_t *string = from[i] == NULL ? L"" : from[i];

			result[i] = str_replace_literal (string, search, replace,
			maximumNumberOfReplaces, &nmatches_sub);
			if (nmatches_sub > 0)
			{
				*nmatches += nmatches_sub;
				(*nstringmatches)++;
			}
		}
		return result.transfer();
	} catch (MelderError) {
		NUMstrings_free (result, lo, hi);
		return 0; 
	}
}


static wchar_t **strs_replace_regexp (wchar_t **from, long lo, long hi,
	const wchar_t *searchRE, const wchar_t *replaceRE, int maximumNumberOfReplaces,
	long *nmatches, long *nstringmatches)
{
	wchar_t **result = NULL;
	try {
		regexp *compiledRE;
		regularExp_CHAR *compileMsg;
		long nmatches_sub = 0;

		if (searchRE == NULL || replaceRE == NULL) return NULL;

		compiledRE = CompileRE ((regularExp_CHAR *) searchRE, &compileMsg, 0);
		if (compiledRE == NULL) Melder_throw ("No regexp ");

		autoNUMvector<wchar_t *> result (lo, hi);

		*nmatches = 0; *nstringmatches = 0;
		for (long i = lo; i <= hi; i++)
		{
			/* Treat a NULL as an empty string */
			const wchar_t *string = from[i] == NULL ? L"" : from[i];
			result [i] = str_replace_regexp (string, compiledRE, replaceRE,
				maximumNumberOfReplaces, &nmatches_sub);
			if (nmatches_sub > 0)
			{
				*nmatches += nmatches_sub;
				(*nstringmatches)++;
			}
		}
		return result.transfer();
	} catch (MelderError) {
		NUMstrings_free (result, lo, hi);
		return 0; 
	}
}

wchar_t **strs_replace (wchar_t **from, long lo, long hi, const wchar_t *search, const wchar_t *replace,
	int maximumNumberOfReplaces, long *nmatches, long *nstringmatches, int use_regexp)
{
	if (use_regexp) return strs_replace_regexp (from, lo, hi, search,
		replace, maximumNumberOfReplaces, nmatches, nstringmatches);
	else return strs_replace_literal (from, lo, hi, search, replace,
		maximumNumberOfReplaces, nmatches, nstringmatches);
}

void NUMdmatrix_printMatlabForm (double **m, long nr, long nc, const wchar_t *name)
{
	long i, j, k, npc = 5;
	ldiv_t n = ldiv (nc, npc);

	MelderInfo_open ();
	MelderInfo_write2 (name, L"=[");
	for (i = 1; i <= nr; i++)
	{
		for (j = 1; j <= n.quot; j++)
		{
			for (k = 1; k <= npc; k++)
			{
				MelderInfo_write2 (Melder_double (m[i][(j-1)*npc+k]), (k < npc ? L", " : L""));
			}
			MelderInfo_write1 (j < n.quot ? L",\n" : L"");
		}

		for (k = 1; k <= n.rem; k++)
		{
			MelderInfo_write2 (Melder_double (m[i][n.quot*npc + k]), (k < n.rem ? L", " : L""));
		}
		MelderInfo_write1 (i < nr ? L";\n" : L"];\n");
	}
	MelderInfo_close ();
}

double **NUMdmatrix_transpose (double **m, long nr, long nc)
{
	long i = 1, j = 1;
	double **to = NUMdmatrix (1, nc, 1, nr);

	if (to == NULL) return NULL;

	for (i = 1; i <= nr; i++)
	{
		for (j = 1; j <= nc; j++)
		{
			to[j][i] = m[i][j];
		}
	}
	return to;
}

void NUMcentreRows (double **a, long rb, long re, long cb, long ce)
{
	long i, j;
	for (i=rb; i <= re; i++)
	{
		double rowmean = 0;
		for (j=cb; j <= ce; j++) rowmean += a[i][j];
		rowmean /= (ce - cb + 1);
		for (j=cb; j <= ce; j++) a[i][j] -= rowmean;
	}
}

void NUMcentreColumns (double **a, long rb, long re, long cb, long ce, double *centres)
{
	for (long j = cb; j <= ce; j++)
	{
		double colmean = 0;
		for (long i = rb; i <= re; i++) colmean += a[i][j];
		colmean /= (re - rb + 1);
		for (long i = rb; i <= re; i++) a[i][j] -= colmean;
		if (centres != NULL) centres[j - cb + 1] = colmean;
	}
}

void NUMdoubleCentre (double **a, long rb, long re, long cb, long ce)
{
	NUMcentreRows (a, rb, re, cb, ce);
	NUMcentreColumns (a, rb, re, cb, ce, NULL);
}

void NUMnormalizeColumns (double **a, long nr, long nc, double norm)
{
	long i, j; double s;
	Melder_assert (norm > 0);
	for (j=1; j <= nc; j++)
	{
		for (s=0, i=1; i <= nr; i++) s += a[i][j] * a[i][j];
		if (s <= 0) continue;
		s = sqrt (norm / s);
		for (i=1; i <= nr; i++) a[i][j] *= s;
	}
}

void NUMnormalizeRows (double **a, long nr, long nc, double norm)
{
	long i, j; double s;
	Melder_assert (norm > 0);
	for (i=1; i <= nr; i++)
	{
		for (s=0, j=1; j <= nc; j++) s += a[i][j] * a[i][j];
		if (s <= 0) continue;
		s = sqrt (norm / s);
		for (j=1; j <= nc; j++) a[i][j] *= s;
	}
}

void NUMnormalize (double **a, long nr, long nc, double norm)
{
	double sq; long i, j;
	Melder_assert (norm > 0);
	for (sq=0, i=1; i <= nr; i++)
	{
		for (j=1; j <= nc; j++) sq += a[i][j] * a[i][j];
	}
	if (sq <= 0) return;
	norm = sqrt (norm / sq);
	for (i=1; i <= nr; i++)
	{
		for (j=1; j <= nc; j++) a[i][j] *= norm;
	}
}

void NUMstandardizeColumns (double **a, long rb, long re, long cb, long ce)
{
	long i, j, n = re - rb + 1;
	if (n < 2) return;
	for (j=cb; j <= ce; j++)
	{
		double ep = 0, s = 0, ave, sdev, var = 0;
		for (i=rb; i <= re; i++) s += a[i][j];
		ave = s / n;
		for (i=rb; i <= re; i++)
		{
			s = a[i][j] - ave;
			ep += s;
			var += s * s;
		}
		if (ave != 0) for (i=rb; i <= re; i++) a[i][j] -= ave;
		if (var > 0)
		{
			var = (var - ep * ep / n) / (n - 1);
			sdev = sqrt (var);
			for (i=rb; i <= re; i++) a[i][j] /= sdev;
		}
	}
}

void NUMstandardizeRows (double **a, long rb, long re, long cb, long ce)
{
	long i, j, n = ce - cb + 1;
	if (n < 2) return;
	for (i=rb; i <= re; i++)
	{
		double ep = 0, s = 0, ave, sdev, var = 0;
		for (j=cb; j <= ce; j++) s += a[i][j];
		ave = s / n;
		for (j=cb; j <= ce; j++)
		{
			s = a[i][j] - ave;
			ep += s;
			var += s * s;
		}
		if (ave != 0) for (j=cb; j <= ce; j++) a[i][j] -= ave;
		if (var > 0)
		{
			var = (var - ep * ep / n) / (n - 1);
			sdev = sqrt (var);
			for (j=cb; j <= ce; j++) a[i][j] /= sdev;
		}
	}
}

void NUMaverageColumns (double **a, long rb, long re, long cb, long ce)
{
	long i, j, n = re - rb + 1;
	if (n < 2) return;
	for (j = cb; j <= ce; j++)
	{
		double ave = 0;
		for (i = rb; i <= re; i++) ave += a[i][j];
		ave /= n;
		for (i = rb; i <= re; i++) a[i][j] = ave;
	}

}

void NUMcolumn_avevar (double **a, long nr, long nc, long icol,
	double *average, double *variance)
{
	long i;
	double eps = 0, mean = 0, var = 0;

	Melder_assert (nr > 0 && nc > 0 && icol > 0 && icol <= nc);

	for (i = 1; i <= nr; i++)
	{
		mean += a[i][icol];
	}

	mean /= nr;

	if (average != NULL) *average = mean;

	if (variance == NULL) return;

	if (nr > 1)
	{
		for (i = 1; i <= nr; i++)
		{
			double s = a[i][icol] - mean;
			eps += s;
			var += s * s;
		}

		var = (var - eps * eps / nr) / (nr -1);
	}
	else
	{
		var = NUMundefined;
	}

	*variance = var;
}

void NUMcolumn2_avevar (double **a, long nr, long nc, long icol1, long icol2,
	double *average1, double *variance1, double *average2, double *variance2,
	double *covariance)
{
	long i, ndf = nr - 1;
	double eps1 = 0, eps2 = 0, mean1 = 0, mean2 = 0;
	double var1 = 0, var2 = 0, covar = 0;

	Melder_assert (icol1 > 0 && icol1 <= nc && icol2 > 0 && icol2 <= nc);

	for (i = 1; i <= nr; i++)
	{
		mean1 += a[i][icol1];
		mean2 += a[i][icol2];
	}

	mean1 /= nr;
	mean2 /= nr;

	if (average1 != NULL) *average1 = mean1;
	if (average2 != NULL) *average2 = mean2;

	if (variance1 == NULL && variance2 == NULL && covariance == NULL) return;

	if (nr > 1)
	{
		for (i = 1; i <= nr; i++)
		{
			double s1 = a[i][icol1] - mean1;
			double s2 = a[i][icol2] - mean2;
			eps1 += s1;
			eps2 += s2;
			var1 += s1 * s1;
			var2 += s2 * s2;
			covar += s1 * s2;
		}

		var1 = (var1 - eps1 * eps1 / nr) / ndf;
		var2 = (var2 - eps2 * eps2 / nr) / ndf;
		covar /= ndf;
	}
	else
	{
		var1 = NUMundefined;
		var2 = NUMundefined;
		covar = NUMundefined;
	}

	if (variance1 != NULL) *variance1 = var1;
	if (variance2 != NULL) *variance2 = var2;
	if (covariance != NULL) *covariance = covar;
	if (icol1 == icol2) *covariance = *variance1;
}

void NUMcovarianceFromColumnCentredMatrix (double **x, long nrows, long ncols, long ndf, double **covar)
{
		if (ndf < 0 || nrows - ndf < 1 || covar == 0) Melder_throw ("Invalid arguments.");
		for (long i = 1; i <= ncols; i++)
		{
			for (long j = i; j <= ncols; j++)
			{
				double sum = 0;
				for (long k = 1; k <= nrows; k++)
				{
					sum += x[k][i] * x[k][j];
				}
				covar[i][j] = covar[j][i] = sum / (nrows - ndf);
			}
		}
}

double NUMmultivariateKurtosis (double **x, long nrows, long ncols, int method)
{
	double kurt = NUMundefined;
	if (nrows < 5) return kurt;
	autoNUMvector<double> mean (1, ncols);
	autoNUMmatrix<double> covar (1, ncols, 1, ncols);

	NUMcentreColumns (x, 1, nrows, 1, ncols, mean.peek());
	NUMcovarianceFromColumnCentredMatrix (x, nrows, ncols, 1, covar.peek());
	if (method == 1) // Schott (2001, page 33)
	{
		kurt = 0;
		for (long l = 1; l <= ncols; l++)
		{
			double zl = 0, wl, sll2 = covar[l][l] * covar[l][l];
			for (long j = 1; j <= nrows; j++)
			{
				double d = x[j][l] - mean[l], d2 = d * d;
				zl += d2 * d2;
			}
			zl = (zl - 6 * sll2) / (nrows - 4);
			wl = (sll2 - zl / nrows) * nrows / (nrows - 1);
			kurt += zl / wl;
		}
		kurt = kurt / (3 * ncols) - 1;
	}
	return kurt;
}

/* obsolete 20080121

#define HUBER_MAD NUMmad_f
#define HUBER_STATISTICS_HUBER NUMstatistics_huber_f
#define HUBER_DATA_TYPE float
#define HUBER_VECTOR  NUMfvector
#define HUBER_VECTOR_FREE  NUMfvector_free
#define HUBER_QUANTILE NUMquantile_f
#define HUBER_SORT NUMsort_f
#include "NUMhuber_core.h"
#undef HUBER_MAD
#undef HUBER_STATISTICS_HUBER
#undef HUBER_DATA_TYPE
#undef HUBER_VECTOR
#undef HUBER_VECTOR_FREE
#undef HUBER_QUANTILE
#undef HUBER_SORT
*/

void eigenSort (double d[], double **v, long n, int sort)
{
    long i, j, k;
	if (sort == 0) return;
    for (i=1; i < n; i++)
    {
        double temp = d[k=i];
        if (sort > 0)
		{
			for (j=i+1; j <= n; j++)
			{
				if (d[j] > temp) temp = d[k=j];
			}
		}
        else
		{
			for (j=i+1; j <= n; j++)
			{
				if (d[j] < temp) temp = d[k=j];
			}
		}
        if (k != i)
        {
            d[k] = d[i];
			d[i] = temp;
            if (v)
			{
				for (j=1; j <= n; j++)
				{
					temp = v[j][i];
					v[j][i] = v[j][k];
					v[j][k] = temp;
				}
			}
        }
    }
}

int NUMstrcmp (const char *s1, const char *s2)
{
	if (s1 == NULL || s1[0] == '\0')
	{
		if (s2 != NULL && s2[0] != '\0') return -1;
		else return 0;
	}
	else
	{
		if (s2 == NULL) return +1;
		else return strcmp (s1, s2);
	}
}

void NUMlocate_f (float *xx, long n, float x, long *index)
{
	long ju = n + 1, jm, jl = 0;
	int ascend = xx[n] >= xx[1];

	while (ju - jl > 1)
	{
		jm = (ju + jl) / 2;
		if ((x >= xx[jm]) == ascend) jl = jm; else ju = jm;
	}
	if (x == xx[1]) *index = 1;
	else if (x == xx[n]) *index = n - 1;
	else *index = jl;
}

void NUMlocate (double *xx, long n, double x, long *index)
{
	long ju = n + 1, jm, jl = 0;
	int ascend = xx[n] >= xx[1];

	while (ju - jl > 1)
	{
		jm = (ju + jl) / 2;
		if ((x >= xx[jm]) == ascend) jl = jm; else ju = jm;
	}
	if (x == xx[1]) *index = 1;
	else if (x == xx[n]) *index = n - 1;
	else *index = jl;
}

/*
	The following algorithm for monotone regession is on the average
	3.5 times faster than
	Kruskal's algorithm for monotone regression (and much simpler).
	Regression is ascending
*/
void NUMmonotoneRegression (const double x[], long n, double xs[])
{
	double sum;
	double xt = NUMundefined; // Only to stop gcc complaining "may be used unitialized"
	//
	long i, j, nt;

	for (i = 1; i <= n; i++)
	{
		xs[i] = x[i];
	}

	for (i = 2; i <= n; i++)
	{
		if (xs[i] >= xs[i-1]) continue;
		sum = xs[i];
		nt = 1;
		for (j = 1; j <= i - 1; j++)
		{
			sum += xs[i-j]; nt++;
			xt = sum / nt; // i >= 2 -> xt always gets a value
			if (j < i-1 && xt >= xs[i-j-1]) break;
		}
		for (j = i - nt + 1; j <= i; j++)
		{
			xs[j] = xt;
		}
	}
}

float NUMfvector_getNorm1 (const float v[], long n)
{
	float norm = 0; long i;
	for (i=1; i <= n; i++) norm += fabs (v[i]);
	return norm;
}

float NUMfvector_getNorm2 (const float v[], long n)
{
	float norm = 0; long i;
	for (i=1; i <= n; i++) norm += v[i] * v[i];
	return sqrt (norm);
}

float NUMfvector_normalize1 (float v[], long n)
{
	float norm = 0; long i;
	for (i=1; i <= n; i++) norm += fabs (v[i]);
	for (i=1; i <= n; i++) v[i] /= norm;
	return norm;
}

float NUMfvector_normalize2 (float v[], long n)
{
	float norm = 0; long i;
	for (i=1; i <= n; i++) norm += v[i] * v[i];
	norm = sqrt (norm);
	for (i=1; i <= n; i++) v[i] /= norm;
	return norm;
}
double NUMdvector_getNorm1 (const double v[], long n)
{
	double norm = 0; long i;
	for (i=1; i <= n; i++) norm += fabs (v[i]);
	return norm;
}

double NUMdvector_getNorm2 (const double v[], long n)
{
	double norm = 0; long i;
	for (i=1; i <= n; i++) norm += v[i] * v[i];
	return sqrt (norm);
}

double NUMdvector_normalize1 (double v[], long n)
{
	double norm = 0; long i;
	for (i=1; i <= n; i++) norm += fabs (v[i]);
	if (norm > 0) for (i=1; i <= n; i++) v[i] /= norm;
	return norm;
}

double NUMdvector_normalize2 (double v[], long n)
{
	double norm = 0; long i;
	for (i = 1; i <= n; i++) norm += v[i] * v[i];
	norm = sqrt (norm);
	if (norm > 0) for (i = 1; i <= n; i++) v[i] /= norm;
	return norm;
}

#undef TINY

void NUMcholeskySolve (double **a, long n, double d[], double b[], double x[])
{
	long i, k;
	double sum;

	for (i=1; i <= n; i++) /* Solve L.y=b */
	{
		for (sum = b[i], k = i - 1; k >= 1; k--)
		{
			sum -= a[i][k] * x[k];
		}
		x[i] = sum / d[i];
	}
	for (i = n; i >= 1; i--) /* Solve L^T.x=y */
	{
		for (sum = x[i], k = i +1; k <= n; k++)
		{
			sum -= a[k][i] * x[k];
		}
		x[i] = sum / d[i];
	}
}

void NUMdeterminant_cholesky (double **a, long n, double *lnd)
{
	// Save the diagonal
	autoNUMvector<double> d (1, n);
	for (long i = 1; i <= n; i++) d[i] = a[i][i];

	//	 Cholesky decomposition in lower, leave upper intact

	char uplo = 'U';
	long lda = n, info;
	NUMlapack_dpotf2 (&uplo, &n, &a[1][1], &lda, &info);
	if (info != 0) Melder_throw ("Cannot determine Cholesky decomposition.");

	// Determinant from diagonal, restore diagonal
	*lnd = 0;
	for (long i = 1; i <= n; i++)
	{
		*lnd += log (a[i][i]);
		a[i][i] = d[i];
	}
	*lnd *= 2; // because A = L . L'

	// Restore lower from upper */

	for (long i = 1; i < n; i++)
	{
		for (long j = i + 1; j <= n; j++) a[j][i] = a[i][j];
	}
}

void NUMlowerCholeskyInverse (double **a, long n, double *lnd)
{

	char uplo = 'U', diag = 'N';
	long info;

	// Cholesky decomposition in lower, leave upper intact
	// Fortran storage -> use uplo='U' to get 'L'.

	(void) NUMlapack_dpotf2 (&uplo, &n, &a[1][1], &n, &info);
	if (info != 0) Melder_throw ("dpotf2 fails.");

	// Determinant from diagonal, diagonal is now sqrt (a[i][i]) !

	if (lnd != NULL)
	{
		*lnd = 0;
		for (long i = 1; i <= n; i++)
		{
			*lnd += log (a[i][i]);
		}
		*lnd *= 2; /* because A = L . L' */
	}

	// Get the inverse */

	(void) NUMlapack_dtrtri (&uplo, &diag, &n, &a[1][1], &n, &info);
	if (info != 0) Melder_throw ("dtrtri fails.");
}

double **NUMinverseFromLowerCholesky (double **m, long n)
{
	autoNUMmatrix<double> r (1, n, 1, n);
	for (long i = 1; i <= n; i++)
	{
		for (long j = 1; j <= i; j++)
		{
			double sum = 0;
			for (long k = i; k <= n; k++)
			{
				sum += m[k][i] * m[k][j];
			}
			r[i][j] = r[j][i] = sum;
		}
	}
	return r.transfer();
}

double NUMmahalanobisDistance_chi (double **linv, double *v, double *m, long nr, long n)
{
	double chisq = 0;
	if (nr == 1) // 1xn matrix
	{
		for (long j = 1; j <= n; j++)
		{
			double t = linv[1][j] * (v[j] - m[j]);
			chisq += t * t;
		}
	}
	else // nxn matrix
	{
		for (long i = n; i > 0; i--)
		{
			double t = 0;
			for (long j = 1; j <= i; j++)
			{
				t += linv[i][j] * (v[j] - m[j]);
			}
			chisq += t * t;
		}
	}
	return chisq;
}

double NUMtrace (double **a, long n)
{
	double trace = 0;
	for (long i = 1; i <= n; i++) trace += a[i][i];
	return trace;
}

double NUMtrace2 (double **a1, double **a2, long n)
{
	double trace = 0;
	for (long i = 1; i <= n; i++) for (long k = 1; k <= n; k++) trace += a1[i][k] * a2[k][i];
	return trace;
}

void NUMeigensystem (double **a, long n, double **evec, double eval[])
{
		autoEigen me = Thing_new (Eigen);
		Eigen_initFromSymmetricMatrix (me.peek(), a, n); therror
		if (evec) NUMdmatrix_copyElements (my eigenvectors, evec, 1, n, 1, n);
		if (eval) NUMdvector_copyElements (my eigenvalues, eval, 1, n);
}

void NUMdominantEigenvector (double **mns, long n, double *q, double *lambda, double tolerance)
{
	autoNUMvector<double> z (1, n);

	double val, cval = 0;
	for (long k = 1; k <= n; k++)
	{
		for (long l = 1; l <= n; l++) { cval += q[k] * mns[k][l] * q[l]; }
	}
	if (cval == 0) Melder_throw ("Zero matrices ??");
	
	long iter = 0; 
	do
	{
		double znorm2 = 0;
		for (long l = 1; l <= n; l++)
		{
			z[l] = 0;
			for (long k = 1; k <= n; k++) { z[l] += mns[l][k] * q[k]; }
		}
		
		for (long k = 1; k <= n; k++)  { znorm2 += z[k] * z[k]; }
		znorm2 = sqrt (znorm2);
		
		for (long k = 1; k <= n; k++) { q[k] = z[k] / znorm2; }
		
		val = cval; cval = 0;
		
		for (long k = 1; k <= n; k++)
		{
			for (long l = 1; l <= n; l++) { cval += q[k] * mns[k][l] * q[l]; }
		}
		
	} while (fabs(cval - val) > tolerance || ++iter < 30);
	*lambda = cval;
}

void NUMprincipalComponents (double **a, long n, long nComponents, double **pc)
{
	autoNUMmatrix<double> evec (1, n, 1, n);
	NUMeigensystem (a, n, evec.peek(), NULL);
	for (long i = 1; i <= n; i++)
	{
		for (long j = 1; j <= nComponents; j++)
		{
			double s = 0;
			for (long k = 1; k <= n; k++)
			{
				s += a[k][i] * evec[k][j]; /* times sqrt(eigenvalue) ?? */
			}
			pc[i][j] = s;
		}
	}
}

void NUMdmatrix_into_principalComponents (double **m, long nrows, long ncols, long numberOfComponents, double **pc)
{
	Melder_assert (numberOfComponents > 0 && numberOfComponents <= ncols);
	autoNUMmatrix<double> mc (NUMdmatrix_copy (m, 1, nrows, 1, ncols), 1, 1);

	/*NUMcentreColumns (mc, nrows, ncols);*/
	autoSVD svd = SVD_create_d (mc.peek(), nrows, ncols);
	for (long i = 1; i <= nrows; i++)
	{
		for (long j = 1; j <= numberOfComponents; j++)
		{
			pc[i][j] = 0;
			for (long k = 1; k <= ncols; k++)
			{
				pc[i][j] += svd -> v[k][j] * m[i][k];
			}
		}
	}
}

void NUMpseudoInverse (double **y, long nr, long nc, double **yinv, double tolerance)
{
	autoSVD me = SVD_create_d (y, nr, nc);

	(void) SVD_zeroSmallSingularValues (me.peek(), tolerance);
	for (long i = 1; i <= nc; i++)
	{
		for (long j = 1; j <= nr; j++)
		{
			double s = 0;
			for (long k = 1; k <= nc; k++)
			{
				if (my d[k] != 0) s += my v[i][k] * my u[j][k] / my d[k];
			}
			yinv[i][j] = s;
		}
	}
}

long NUMsolveQuadraticEquation (double a, double b, double c, double *x1, double *x2)
{
	return gsl_poly_solve_quadratic (a, b, c, x1, x2);
}

void NUMsolveEquation (double **a, long nr, long nc, double *b, double tolerance, double *result)
{
	double tol = tolerance > 0 ? tolerance : NUMfpp -> eps * nr;

	if (nr <= 0 || nc <= 0) Melder_throw ("Negative dimensions");

	autoSVD me = SVD_create_d (a, nr, nc);
	SVD_zeroSmallSingularValues (me.peek(), tol);
	SVD_solve (me.peek(), b, result);
}


void NUMsolveEquations (double **a, long nr, long nc, double **b, long ncb, double tolerance, double **x)
{
	double tol = tolerance > 0 ? tolerance : NUMfpp -> eps * nr;

	if (nr <= 0 || nc <= 0) Melder_throw ("Negative dimensions");

	autoSVD me = SVD_create_d (a, nr, nc);
	autoNUMvector<double> bt (1, nr + nc);
	double *xt = & bt[nr];

	SVD_zeroSmallSingularValues (me.peek(), tol);

	for (long k = 1; k <= ncb; k++)
	{
		for (long j = 1; j <= nr; j++)
		{
			bt[j] = b[j][k];
		}

		SVD_solve (me.peek(), bt.peek(), xt);

		for (long j = 1; j <= nc; j++)
		{
			x[j][k] = xt[j];
		}
	}
}


void NUMsolveNonNegativeLeastSquaresRegression (double **m, long nr, long nc, double *d, double tol, long itermax, double *b)
{
	double difsq, difsqp = 0;

	for (long iter = 1; iter <= itermax; iter++)
	{
		/*
			Fix all weights except b[j]
		*/

		for (long j = 1; j <= nc; j++)
		{
			double mjr = 0, mjmj = 0;
			for (long i = 1; i <= nr; i++)
			{
				double ri = d[i], mij = m[i][j];
				for (long l = 1; l <= nc; l++)
				{
					if (l != j) ri -= b[l] * m[i][l];
				}
				mjr += mij * ri;
				mjmj += mij * mij;
			}
			b[j] = mjr / mjmj;
			if (b[j] < 0) b[j] = 0;
		}

		/*
			Calculate t(b) and compare with previous result.
		*/

		difsq = 0;
		for (long i = 1; i <= nr; i++)
		{
			double dmb = d[i];
			for (long j = 1; j <= nc; j++)
			{
				dmb -= m[i][j] * b[j];
			}
			difsq += dmb * dmb;
		}
		if (fabs (difsq - difsqp) / difsq < tol) break;
		difsqp = difsq;
	}
}

struct nr_struct { double *y, *delta; };

/*
	f (lambda) = sum (y[i]^2 delta[i] / (delta[i]-lambda)^2, i=1..3)
	f'(lambda) = 2 * sum (y[i]^2 delta[i] / (delta[i]-lambda)^3, i=1..3)
*/

static void nr_func (double x, double *f, double *df, void *data)
{
	struct nr_struct *me = (struct nr_struct *) data;
	*f = *df = 0;
	for (long i = 1; i <= 3; i++)
	{
		double t1 = (my delta[i] - x);
		double t2 = my y[i] / t1;
		double t3 = t2 * t2 * my delta[i];
		*f  += t3;
		*df += t3 * 2 / t1;
	}
}

void NUMsolveConstrainedLSQuadraticRegression (double **o, const double d[], long n, double *alpha, double *gamma)
{
		long n3 = 3, info;
		double eps = 1e-5, t1, t2, t3;

		autoNUMmatrix<double> ftinv (1, n3, 1, n3);
		autoNUMmatrix<double> b (1, n3, 1, n3);
		autoNUMmatrix<double> g (1, n3, 1, n3);
		autoNUMmatrix<double> p (1, n3, 1, n3);
		autoNUMvector<double> delta (1, n3);
		autoNUMmatrix<double> ftinvp (1, n3, 1, n3);
		autoNUMmatrix<double> ptfinv (1, n3, 1, n3);
		autoNUMvector<double> otd (1, n3);
		autoNUMmatrix<double> ptfinvc (1, n3, 1, n3);
		autoNUMvector<double> y (1, n3);
		autoNUMvector<double> w (1, n3);
		autoNUMvector<double> chi (1, n3);
		autoNUMvector<double> diag (1, n3);

		// Construct O'.O     [1..3][1..3].

		for (long i = 1; i <= n3; i++)
		{
			for (long j = 1; j <= n3; j++)
			{
				for (long k = 1; k <= n; k++)
				{
					ftinv[i][j] += o[k][i] * o[k][j];
				}
			}
		}

		// Get lower triangular decomposition from O'.O and
		// get F'^-1 from it (eq. (2)) (F^-1 not done ????)

		char uplo = 'U';
		(void) NUMlapack_dpotf2 (&uplo, &n3, &ftinv[1][1], &n3, &info);
		if (info != 0) Melder_throw ("dpotf2 fails.");
		ftinv[1][2] = ftinv[1][3] = ftinv[2][3] = 0;

		// Construct G and its eigen-decomposition (eq. (4,5))
		// Sort eigenvalues (& eigenvectors) ascending.

		b[3][1] = b[1][3] = -0.5; b[2][2] = 1;

		// G = F^-1 B (F')^-1 (eq. 4)

		for (long i = 1; i <= 3; i++)
		{
			for (long j = 1; j <= 3; j++)
			{
				for (long k = 1; k <= 3; k++)
				{
					if (ftinv[k][i] != 0)
					{
						for (long l = 1; l <= 3; l++)
						{
							g[i][j] += ftinv[k][i] * b[k][l] * ftinv[l][j];
						}
					}
				}
			}
		}

		// G's eigen-decomposition with eigenvalues (assumed ascending). (eq. 5)

		NUMeigensystem (g.peek(), 3, p.peek(), delta.peek());

		NUMsort_d (3, delta.peek()); /* ascending */

		// Construct y = P'.F'.O'.d ==> Solve (F')^-1 . P .y = (O'.d)    (page 632)
		// Get P'F^-1 from the transpose of (F')^-1 . P

		for (long i = 1; i <= 3; i++)
		{
			for (long j = 1; j <= 3; j++)
			{
				if (ftinv[i][j] != 0)
				{
					for (long k = 1; k <= 3; k++)
					{
						ftinvp[i][k] += ftinv[i][j] * p[3+1-j][k]; /* is sorted desc. */
					}
				}
			}
			for (long k = 1; k <= n; k++)
			{
				otd[i] += o[k][i] * d[k];
			}
		}

		for (long i = 1; i <= 3; i++)
		{
			for (long j = 1; j <= 3; j++)
			{
				ptfinvc[j][i] = ptfinv[j][i] = ftinvp[i][j];
			}
		}

		NUMsolveEquation (ftinvp.peek(), 3, 3, otd.peek(), 1e-6, y.peek());

		// The solution (3 cases)

		if (fabs (y[1]) < eps)
		{
			// Case 1: page 633

			t2 = y[2] / (delta[2] - delta[1]);
			t3 = y[3] / (delta[3] - delta[1]);
			/* +- */
			w[1] = sqrt (- delta[1] * (t2 * t2 * delta[2] + t3 * t3 * delta[3]));
			w[2] = t2 * delta[2];
			w[3] = t3 * delta[3];

			NUMsolveEquation (ptfinv.peek(), 3, 3, w.peek(), 1e-6, chi.peek());

			w[1] = -w[1];
			if (fabs (chi[3] / chi[1]) < eps) NUMsolveEquation (ptfinvc.peek(), 3, 3, w.peek(), 1e-6, chi.peek());
		}
		else if (fabs (y[2]) < eps)
		{
			// Case 2: page 633

			t1 = y[1] / (delta[1] - delta[2]);
			t3 = y[3] / (delta[3] - delta[2]);
			w[1] = t1 * delta[1];
			if((delta[2] < delta[3] &&
				(t2 = (t1 * t1 * delta[1] + t3 * t3 * delta[3])) < eps))
			{
				w[2] = sqrt (- delta[2] * t2); /* +- */
				w[3] = t3 * delta[3];
				NUMsolveEquation (ptfinv.peek(), 3, 3, w.peek(), 1e-6, chi.peek());
				w[2] = -w[2];
				if (fabs (chi[3] / chi[1]) < eps) NUMsolveEquation (ptfinvc.peek(), 3, 3, w.peek(), 1e-6, chi.peek());
			}
			else if (((delta[2] < delta[3] + eps) || (delta[2] > delta[3] - eps))
				&& fabs (y[3]) < eps)
			{
				// choose one value for w[2] from an infinite number

				w[2] = w[1];
				w[3] = sqrt (- t1 * t1 * delta[1] * delta[2] - w[2] * w[2]);
				NUMsolveEquation (ptfinv.peek(), 3, 3, w.peek(), 1e-6, chi.peek());
			}
		}
		else
		{
			// Case 3: page 634 use Newton-Raphson root finder

			struct nr_struct me;
			double xlambda, eps = (delta[2] - delta[1]) * 1e-6;

			me.y = y.peek(); me.delta = delta.peek();

			NUMnrbis (nr_func, delta[1] + eps, delta[2] - eps, & me, & xlambda);

			for (long i = 1; i <= 3; i++)
			{
				w[i] = y[i] / (1 - xlambda / delta[i]);
			}
			NUMsolveEquation (ptfinv.peek(), 3, 3, w.peek(), 1e-6, chi.peek());
		}

		*alpha = chi[1]; *gamma = chi[3];
}

/*
	f (b) = delta - b / (2 alpha) - sum (x[i]^2 / (c[i] - b)^2, i=1..n)
	f'(b) = - 1 / (2 alpha) + 2 * sum (x[i]^2 / (c[i] - b)^3, i=1..n)
*/
struct nr2_struct { long m; double delta, alpha, *x, *c; };

static void nr2_func (double b, double *f, double *df, void *data)
{
	struct nr2_struct *me = (struct nr2_struct *) data;

	*df = - 0.5 / my alpha;
	*f = my delta + *df * b;
	for (long i = 1; i <= my m; i++)
	{
		double c1 = (my c[i] - b);
		double c2 = my x[i] / c1;
		double c2sq = c2 * c2;
		*f -= c2sq; *df += 2 * c2sq / c1;
	}
}

void NUMsolveWeaklyConstrainedLinearRegression (double **f, long n, long m, double phi[], double alpha, double delta, double t[])
{
		autoNUMmatrix<double> u (1, m, 1, m);
		autoNUMvector<double> c (1, m);
		autoNUMvector<double> x (1, n);
		autoNUMvector<long> indx (1, m);
		
		for (long j = 1; j <= m; j++) t[j] = 0;

		autoSVD svd = SVD_create_d (f, n, m);

		if (alpha == 0) SVD_solve (svd.peek(), phi, t); // standard least squares


		// Step 1: Compute U and C from the eigendecomposition F'F = UCU'
		// Evaluate q, the multiplicity of the smallest eigenvalue in C


		double *sqrtc = svd -> d;
		double **ut = svd -> v;
		NUMindexx (sqrtc, m, indx.peek());

		for (long j = m; j > 0; j--)
		{
			double tmp = sqrtc [indx[j]];
			c[m - j + 1] = tmp * tmp;
			for (long k = 1; k <= m; k++)
			{
				u[m - j + 1][k] = ut [indx[j]] [k];
			}
		}

		long q = 1;
		double tol = 1e-6;
		while (q < m && (c[m - q] - c[m]) < tol) q++;

		// step 2: x = U'F'phi

		for (long i = 1; i <= m; i++)
		{
			for (long j=1; j <= m; j++)
			{
				for (long k=1; k <= n; k++)
				{
					x[i] += u[j][i] * f[k][j] * phi[k];
				}
			}
		}

		// step 3:

		struct nr2_struct me;
		me.m = m;
		me.delta = delta;
		me.alpha = alpha;
		me.x = x.peek();
		me.c = c.peek();
		
		double xqsq = 0;
		for (long j = m - q + 1; j <= m; j++)
		{
			xqsq += x[j] * x[j];
		}
		
		long r = m;
		if (xqsq < tol) /* xqsq == 0 */
		{
			double fm, df;
			r = m - q;
			me.m = r;
			nr2_func (c[m], &fm, &df, & me);
			if (fm >= 0) /* step 3.b1 */
			{
				x[r+1] = sqrt (fm);
				for (long j = 1; j <= r; j++)
				{
					x[j] /= c[j] - c[m];
				}
				for (long j = 1; j <= r+1; j++)
				{
					for (long k = 1; k <= r+1; k++)
					{
						t[j] += u[j][k] * x[k];
					}
				}
				return;
			}
			/* else continue with r = m - q */
		}

		// step 3a & 3b2, determine interval lower bound for Newton-Raphson root finder
		
		double xCx = 0;
		for (long j = 1; j <= r; j++)
		{
			xCx += x[j] * x[j] / c[j];
		}
		double b0, bmin = delta > 0 ? - xCx / delta : -2 * sqrt (alpha * xCx);
		double eps = (c[m] - bmin) * tol;

		// find the root of d(psi(b)/db in interval (bmin, c[m])

		NUMnrbis (nr2_func, bmin + eps, c[m] - eps, & me, & b0);

		for (long j = 1; j <= r; j++)
		{
			for (long k = 1; k <= r; k++)
			{
				t[j] += u[j][k] * x[k] / (c[k] - b0);
			}
		}
}

void NUMProcrustes (double **x, double **y, long nPoints, long nDimensions, double **t, double *v, double *s)
{
	int orthogonal = v == 0 || s == 0; /* else similarity transform */

	autoNUMmatrix<double> c (1, nDimensions, 1, nDimensions);
	autoNUMmatrix<double> yc (1, nPoints, 1, nDimensions);
	NUMdmatrix_copyElements (y, yc.peek(), 1, nPoints, 1, nDimensions);
	
	/*
		Reference: Borg & Groenen (1997), Modern multidimensional scaling,
		Springer
		1. Calculate C = X'JY (page 346) for similarity transform
			else X'Y for othogonal (page 341)
			JY amounts to centering the columns of Y.
	*/

	if (! orthogonal) NUMcentreColumns (yc.peek(), 1, nPoints, 1, nDimensions, NULL);
	for (long i = 1; i <= nDimensions; i++)
	{
		for (long j = 1; j <= nDimensions; j++)
		{
			for (long k = 1; k <= nPoints; k++)
			{
				c[i][j] += x[k][i] * yc[k][j];
			}
		}
	}

	// 2. Decompose C by SVD:  C = PDQ' (SVD attribute is Q instead of Q'!)

	autoSVD svd = SVD_create_d (c.peek(), nDimensions, nDimensions);
	double trace = 0;
	for (long i = 1; i <= nDimensions; i++) { trace += svd -> d[i]; }

	if (trace == 0) Melder_throw ("NUMProcrustes: degenerate configuration(s).");

	// 3. T = QP'

	for (long i = 1; i <= nDimensions; i++)
	{
		for (long j = 1; j <= nDimensions; j++)
		{
			t[i][j] = 0;
			for (long k = 1; k <= nDimensions; k++)
			{
				t[i][j] += svd -> v[i][k] * svd -> u[j][k];
			}
		}
	}

	if (! orthogonal)
	{
		autoNUMmatrix<double> xc (1, nPoints, 1, nDimensions);
		NUMdmatrix_copyElements (x, xc.peek(), 1, nPoints, 1, nDimensions);
		autoNUMmatrix<double> yt (1, nPoints, 1, nDimensions);

		// 4. Dilation factor s = (tr X'JYT) / (tr Y'JY)
		// First we need YT.

		for (long i = 1; i <= nPoints; i++)
		{
			for (long j = 1; j <= nDimensions; j++)
			{
				for (long k = 1; k <= nDimensions; k++)
				{
					yt[i][j] += y[i][k] * t[k][j];
				}
			}
		}

		// X'J amount to centering the columns of X

		NUMcentreColumns (xc.peek(), 1, nPoints, 1, nDimensions, NULL);

		// tr X'J YT == tr xc' yt

		double traceXtJYT = 0;
		for (long i = 1; i <= nDimensions; i++)
		{
			for (long j = 1; j <= nPoints; j++)
			{
				traceXtJYT += xc[j][i] * yt[j][i];
			}
		}
		double traceYtJY = 0;
		for (long i = 1; i <= nDimensions; i++)
		{
			for (long j = 1; j <= nPoints; j++)
			{
				traceYtJY += y[j][i] * yc[j][i];
			}
		}

		*s = traceXtJYT / traceYtJY;

		// 5. Translation vector tr = (X - sYT)'1 / nPoints

		for (long i = 1; i <= nDimensions; i++)
		{
			for (long j = 1; j <= nPoints; j++)
			{
				v[i] += x[j][i] - *s * yt[j][i];
			}
			v[i] /= nPoints;
		}
	}
}


void NUMmspline (double knot[], long nKnots, long order, long i, double x, double *y)
{
		long j, nSplines = nKnots - order;
		if (nSplines <= 0) Melder_throw ("No splines.");
		
		// Find the interval where x is located.
		// M-splines of order k have degree k-1.
		// M-splines are zero outside interval [ knot[i], knot[i+order] ).
		// First and last 'order' knots are equal, i.e.,
		// knot[1] = ... = knot[order] && knot[nKnots-order+1] = ... knot[nKnots].

		*y = 0;
		if (i > nSplines || order < 1) Melder_throw ("Combination of order and index not correct.");
		for (j = order; j <= nKnots - order + 1; j++)
		{
			if (x < knot[j]) break;
		}
		if (j < i || (j > i + order) || j == order || j > (nKnots - order + 1)) return;

		// Calculate M[i](x|1,t) according to eq.2.
		
		long ito = i + order - 1;
		autoNUMvector<double> m (i, ito);
		for (long j = i; j <= ito; j++)
		{
			if (x >= knot[j] && x < knot[j+1]) m[j] = 1 / (knot[j+1] - knot[j]);
		}

		// Iterate to get M[i](x|k,t)

		for (long k = 2; k <= order; k++)
		{
			for (long j = i; j <= i + order - k; j++)
			{
				double kj = knot[j], kjpk = knot[j+k];
				if (kjpk > kj)
				{
					m[j] = k * ((x - kj) * m[j] + (kjpk - x) * m[j+1]) / ((k - 1) * (kjpk - kj));
				}
			}
		}
		*y = m[i];
}

void NUMispline (double aknot[], long nKnots, long order, long i, double x, double *y)
{
		long j, orderp1 = order + 1;

		*y = 0;

		for (j = orderp1; j <= nKnots-order; j++)
		{
			if (x < aknot[j]) break;
		}
		j--;
		if (j < i) return;
		if (j > i + order || (j == nKnots - order && x == aknot[j]))
		{
			*y = 1; return;
		}

		// Equation 5 in Ramsay's article contains some errors!!!
		// 1. the interval selection must be 'j-k <= i <= j' instead of
		//	'j-k+1 <= i <= j'
		// 2. the summation index m starts at 'i+1' instead of 'i'

		for (long m = i + 1; m <=j; m++)
		{
			double r;
			NUMmspline (aknot, nKnots, orderp1, m, x, &r);
			*y += (aknot[m+orderp1] - aknot[m]) * r;
		}
		*y /= orderp1;
}

double NUMwilksLambda (double *lambda, long from, long to)
{
	double result = 1;
	for (long i = from; i <= to; i++) result /= (1 + lambda[i]);
	return result;
}

double NUMfactln (int n)
{
	static double table[101];
	if (n < 0) return NUMundefined;
	if (n <= 1) return 0;
	return n > 100 ? NUMlnGamma (n + 1.0) : table[n] ? table[n] :
		(table[n] = NUMlnGamma (n + 1.0));
}

void NUMnrbis (void (*f) (double x, double *fx, double *dfx, void *closure), double x1, double x2, void *closure, double *root)
{
	double df, dx, dxold, fx, fh, fl, tmp, xh, xl, tol;
	long itermax = 60;

	(*f) (x1, &fl, &df, closure);
	if (fl == 0.0)
	{
		*root = x1;
		return;
	}

	(*f) (x2, &fh, &df, closure);
	if (fh == 0.0)
	{
		*root = x2;
		return;
	}

	if ((fl > 0.0 && fh > 0.0) || (fl < 0.0 && fh < 0.0))
	{
		*root = NUMundefined;
		Melder_throw (L"Root must be bracketed.");
	}

	if (fl < 0.0) { xl = x1; xh = x2; }
	else { xh = x1; xl = x2; }

	dxold = fabs (x2 - x1);
	dx = dxold;
	*root = 0.5 * (x1 + x2);
	(*f) (*root, &fx, &df, closure);

	for (long iter = 1; iter <= itermax; iter++)
	{
		if ((((*root - xh) * df - fx) * ((*root - xl) * df - fx) >= 0.0) || (fabs (2.0 * fx) > fabs (dxold * df)))
		{
			dxold = dx;
			dx = 0.5 * (xh - xl);
			*root = xl + dx;
			if (xl == *root) return;
		}
		else
		{
			dxold = dx;
			dx = fx / df;
			tmp = *root;
			*root -= dx;
			if (tmp == *root) return;
		}
		tol = NUMfpp -> eps	* (*root == 0 ? 1 : fabs (*root));
		if (fabs(dx) < tol) return;

		(*f) (*root, &fx, &df, closure);

		if (fx < 0.0) { xl = *root; }
		else { xh = *root; }
	}
	Melder_warning3 (L"NUMnrbis: maximum number of iterations (", Melder_integer (itermax), L") exceeded.");
}

double NUMridders (double (*f) (double x, void *closure), double x1, double x2, void *closure)
{
	/* There is still a problem with this implementation:
		tol may be zero;
	*/
	double x3, x4, d, root = NUMundefined;
	double f1, f2, f3, f4, tol;
	long itermax = 100;

	f1 = f (x1, closure);
	if (f1 == 0.0) return x1;
	if (f1 == NUMundefined) return NUMundefined;
	f2 = f (x2, closure);
	if (f2 == 0.0) return x2;
	if (f2 == NUMundefined) return NUMundefined;
	if ((f1 < 0 && f2 < 0) || (f1 > 0 && f2 > 0))
	{
		Melder_warning1 (L"NUMridders: root must be bracketed.");
		return NUMundefined;
	}

	for (long iter = 1; iter <= itermax; iter++)
	{
		x3 = 0.5 * (x1 + x2);
		f3 = f (x3, closure);
		if (f3 == 0.0) return x3;
		if (f3 == NUMundefined) return NUMundefined;

		/*
			New guess:
			x4 = x3 + (x3 - x1) * sign(f1 - f2) * f3 / sqrt(f3^2 - f1*f2)
		*/

		d = f3 * f3 - f1 * f2;
		if (d < 0.0)
		{
			Melder_warning3 (L"d < 0 in ridders (iter = ", Melder_integer (iter), L").");
			return NUMundefined;
		}

		if (d == 0.0)
		{   /* pb test added because f1 f2 f3 may be 1e-170 or so */
			tol = NUMfpp -> eps * fabs (x3);
			if (iter > 1 && fabs (x3 - root) < tol) return root;
			root = x3;
			/*
			 * Perform bisection.
			 */
			if (f1 > 0.0)
			{   /* falling curve: f1 > 0, f2 < 0 */
				if (f3 > 0.0)
				{
					x1 = x3; f1 = f3;   /* retain invariant: f1 > 0, f2 < 0 */
				}
				else
				{   /* f3 < 0.0 */
					x2 = x3; f2 = f3;   /* retain invariant: f1 > 0, f2 < 0 */
				}
			}
			else
			{   /* rising curve: f1 < 0, f2 > 0 */
				if (f3 > 0.0)
				{
					x2 = x3; f2 = f3;   /* retain invariant: f1 < 0, f2 > 0 */
				}
				else
				{   /* f3 < 0.0 */
					x1 = x3; f1 = f3;   /* retain invariant: f1 < 0, f2 > 0 */
				}
			}
		}
		else
		{
			d = sqrt (d);
			if (isnan (d))
			{   /* pb: square root of denormalized small number fails on some computers */
				tol = NUMfpp -> eps * fabs (x3);
				if (iter > 1 && fabs (x3 - root) < tol) return root;
				root = x3;
				/*
				 * Perform bisection.
				 */
				if (f1 > 0.0)
				{   /* falling curve: f1 > 0, f2 < 0 */
					if (f3 > 0.0)
					{
						x1 = x3; f1 = f3;   /* retain invariant: f1 > 0, f2 < 0 */
					}
					else
					{   /* f3 < 0.0 */
						x2 = x3; f2 = f3;   /* retain invariant: f1 > 0, f2 < 0 */
					}
				}
				else
				{   /* rising curve: f1 < 0, f2 > 0 */
					if (f3 > 0.0)
					{
						x2 = x3; f2 = f3;   /* retain invariant: f1 < 0, f2 > 0 */
					}
					else
					{   /* f3 < 0.0 */
						x1 = x3; f1 = f3;   /* retain invariant: f1 < 0, f2 > 0 */
					}
				}
			}
			else
			{
				d = (x3 - x1) * f3 / d;
				x4 = f1 - f2 < 0 ? x3 - d : x3 + d;
				tol = NUMfpp -> eps * fabs (x4);
				if (iter > 1 && fabs (x4 - root) < tol) return root;
				root = x4;
				f4 = f (x4, closure);
				if (f4 == 0.0) return root;
				if (f4 == NUMundefined) return NUMundefined;
				if ((f1 > f2) == (d > 0.0) /* pb: instead of x3 < x4 */)
				{
					if (SIGN (f3, f4) != f3)
					{
						x1 = x3; f1 = f3;
						x2 = x4; f2 = f4;
					}
					else
					{
						x1 = x4; f1 = f4;
					}
				}
				else
				{
					if (SIGN (f3, f4) != f3)
					{
						x1 = x4; f1 = f4;
						x2 = x3; f2 = f3;
					}
					else
					{
						x2 = x4; f2 = f4;
					}
				}
			}
		}
		if (fabs (x1 - x2) < tol) return root;
	}

	{
		static long nwarnings = 0;
		nwarnings++;
		Melder_warning3 (L"NUMridders: maximum number of iterations (", Melder_integer (itermax), L") exceeded.");
	}
	return root;
}

double NUMlogNormalP (double x, double zeta, double sigma)
{
	return gsl_cdf_lognormal_P (x, zeta, sigma);
}

double NUMlogNormalQ (double x, double zeta, double sigma)
{
	return gsl_cdf_lognormal_Q (x, zeta, sigma);
}

double NUMstudentP (double t, double df)
{
	if (df < 1) return NUMundefined;
	double ib = NUMincompleteBeta (0.5 * df, 0.5, df / (df + t * t));
	if (ib == NUMundefined) return NUMundefined;
	ib *= 0.5;
	return t < 0 ? ib : 1 - ib;
}

double NUMstudentQ (double t, double df)
{
	if (df < 1) return NUMundefined;
	double ib = NUMincompleteBeta (0.5 * df, 0.5, df / (df + t * t));
	if (ib == NUMundefined) return NUMundefined;
	ib *= 0.5;
	return t > 0 ? ib : 1 - ib;
}

double NUMfisherP (double f, double df1, double df2)
{
	if (f < 0 || df1 < 1 || df2 < 1) return NUMundefined;
	double ib = NUMincompleteBeta (0.5 * df2, 0.5 * df1, df2 / (df2 + f * df1));
	if (ib == NUMundefined) return NUMundefined;
	return 1 - ib;
}

double NUMfisherQ (double f, double df1, double df2)
{
	if (f < 0 || df1 < 1 || df2 < 1) return NUMundefined;
	if (Melder_debug == 28) {
		return NUMincompleteBeta (0.5 * df2, 0.5 * df1, df2 / (df2 + f * df1));
	} else {
		double result = gsl_cdf_fdist_Q (f, df1, df2);
		if (isnan (result)) return NUMundefined;
		return result;
	}
}

double NUMinvGaussQ (double p)
{
	double pc = p;
	if (p <= 0 || p >= 1.0) return NUMundefined;
	if (p > 0.5) pc = 1 - p;
	double t = sqrt (- 2.0 * log (pc));
	t -= (2.515517 + (0.802853 + 0.010328 * t) * t) /
		(1.0 + (1.432788 + (0.189269 + 0.001308 * t) * t) * t);
	return p > 0.5 ? -t : t;
}

static double studentQ_func (double x, void *voidParams)
{
	struct pdf1_struct *params = (struct pdf1_struct *) voidParams;
	double q = NUMstudentQ (x, params -> df);
	return q == NUMundefined ? NUMundefined : q - params -> p;
}

double NUMinvStudentQ (double p, double df)
{
	struct pdf1_struct params;
	double pc = p > 0.5 ? 1 - p : p, xmin, xmax = 1, x;

	if (p < 0 || p >= 1) return NUMundefined;

	/*
		Bracket the function f(x) = NUMstudentQ (x, df) - p.
	*/
	for (;;)
	{
		double q = NUMstudentQ (xmax, df);
		if (q == NUMundefined) return NUMundefined;
		if (q < pc) break;
		xmax *= 2;
	}

	xmin = xmax > 1 ? xmax / 2 : 0;

	/*
		Find zero of f(x) with Ridders' method.
	*/
	params. df = df;
	params. p = pc;
	x = NUMridders (studentQ_func, xmin, xmax, & params);
	if (x == NUMundefined) return NUMundefined;

	return p > 0.5 ? -x : x;
}

static double chiSquareQ_func (double x, void *voidParams)
{
	struct pdf1_struct *params = (struct pdf1_struct *) voidParams;
	double q = NUMchiSquareQ (x, params -> df);
	return q == NUMundefined ? NUMundefined : q - params -> p;
}

double NUMinvChiSquareQ (double p, double df)
{
	struct pdf1_struct params;
	double xmin, xmax = 1;

	if (p < 0 || p >= 1) return NUMundefined;

	/*
		Bracket the function f(x) = NUMchiSquareQ (x, df) - p.
	*/
	for (;;)
	{
		double q = NUMchiSquareQ (xmax, df);
		if (q == NUMundefined) return NUMundefined;
		if (q < p) break;
		xmax *= 2;
	}
	xmin = xmax > 1 ? xmax / 2 : 0;

	/*
		Find zero of f(x) with Ridders' method.
	*/
	params. df = df;
	params. p = p;
	return NUMridders (chiSquareQ_func, xmin, xmax, & params);
}

static double fisherQ_func (double x, void *voidParams)
{
	struct pdf2_struct *params = (struct pdf2_struct *) voidParams;
	double q = NUMfisherQ (x, params -> df1, params -> df2);
	return q == NUMundefined ? NUMundefined : q - params -> p;
}

double NUMinvFisherQ (double p, double df1, double df2)
{
	if (p <= 0.0 || p > 1.0 || df1 < 1 || df2 < 1) return NUMundefined;
	if (Melder_debug == 29) {
		//if (p == 1.0) return 0.0;
		return gsl_cdf_fdist_Qinv (p, df1, df2);
	} else {
		struct pdf2_struct params;
		double top = 1000.0;
		if (p == 1.0) return 0.0;
		params. p = p;
		params. df1 = df1;
		params. df2 = df2;
		for (;;)
		{
			double q = NUMfisherQ (top, df1, df2);
			if (q == NUMundefined) return NUMundefined;
			if (q < p) break;
			if (top > 0.9e300) return NUMundefined;
			top *= 1e9;
		}
		return NUMridders (fisherQ_func, 0.0, p > 0.5 ? 2.2 : top, & params);
	}
}

double NUMbeta2 (double z, double w) {
	gsl_sf_result result;
	int status = gsl_sf_beta_e(z, w, &result);
	return status == GSL_SUCCESS ? result.val : NUMundefined;
}

double NUMlnBeta (double a, double b)
{
	gsl_sf_result result;
	int status = gsl_sf_lnbeta_e (a, b, &result);
	return status == GSL_SUCCESS ? result.val : NUMundefined;
}

double NUMnormalityTest_HenzeZirkler (double **data, long n, long p, double *beta, double *tnb, double *lnmu, double *lnvar)
{
	if (*beta <= 0) *beta = (1.0 / sqrt (2)) * pow ((1.0 + 2 * p ) / 4, 1.0 / (p + 4 )) * pow (n, 1.0 / (p + 4));
	double p2 = p / 2.0;
	double beta2 = *beta * *beta, beta4 = beta2 * beta2, beta8 = beta4 * beta4;
	double gamma = 1 + 2 * beta2, gamma2 = gamma * gamma, gamma4 = gamma2 * gamma2;
	double delta = 1.0 + beta2 * (4 + 3 * beta2), delta2 = delta * delta;
	double prob = NUMundefined;

	*tnb = *lnmu = *lnvar = NUMundefined;

	if (n < 2 || p < 1) return prob;

	autoNUMvector<double> zero (1, p);
	autoNUMmatrix<double> covar (1, p, 1, p);
	autoNUMmatrix<double> x (NUMdmatrix_copy (data, 1, n, 1, p), 1, 1);

	NUMcentreColumns (x.peek(), 1, n, 1, p, NULL); // x - xmean

	NUMcovarianceFromColumnCentredMatrix (x.peek(), n, p, 0, covar.peek());

	try { 
		NUMlowerCholeskyInverse (covar.peek(), p, NULL);
		double djk, djj, sumjk = 0, sumj = 0;
		double b1 = beta2 / 2, b2 = b1 / (1.0 + beta2);
		/* Heinze & Wagner (1997), page 3
			We use d[j][k] = ||Y[j]-Y[k]||^2 = (Y[j]-Y[k])'S^(-1)(Y[j]-Y[k])
			So d[j][k]= d[k][j] and d[j][j] = 0
		*/
		for (long j = 1; j <= n; j++)
		{
			for (long k = 1; k < j; k++)
			{
				djk = NUMmahalanobisDistance_chi (covar.peek(), x[j], x[k], p, p);
				sumjk += 2 * exp (-b1 * djk); // factor 2 because d[j][k] == d[k][j]
			}
			sumjk += 1; // for k == j
			djj = NUMmahalanobisDistance_chi (covar.peek(), x[j], zero.peek(), p, p);
			sumj += exp (-b2 * djj);
		}
		*tnb = (1.0 / n) * sumjk - 2.0 * pow (1.0 + beta2, - p2) * sumj + n * pow (gamma, - p2); // n *
	} catch (MelderError) {
		Melder_clearError ();
		*tnb = 4 * n;
	}

	double mu = 1.0 - pow (gamma, -p2) * (1.0 + p * beta2 / gamma + p * (p + 2) * beta4 / (2 * gamma2));
	double var = 2.0 * pow (1 + 4 * beta2, -p2)
		+ 2.0 * pow (gamma,  -p) * (1.0 + 2 * p * beta4 / gamma2  + 3 * p * (p + 2) * beta8 / (4 * gamma4))
		- 4.0 * pow (delta, -p2) * (1.0 + 3 * p * beta4 / (2 * delta) + p * (p + 2) * beta8 / (2 * delta2));
	double mu2 = mu * mu;
	*lnmu = log (sqrt (mu2 * mu2 /(mu2 + var)));
	*lnvar = sqrt (log ((mu2 + var) / mu2));
	prob = NUMlogNormalQ (*tnb, *lnmu, *lnvar);
	return prob;
}

/*************** Hz <--> other freq reps *********************/

double NUMmelToHertz3 (double mel)
{
	if (mel < 0) return NUMundefined;
	return mel < 1000 ? mel : 1000 * (exp (mel * log10(2) / 1000) - 1);
}

double NUMhertzToMel3 (double hz)
{
	if (hz < 0) return NUMundefined;
	return hz < 1000 ? hz : 1000 * log10 (1 + hz / 1000) / log10 (2);
}

double NUMmelToHertz2 (double mel)
{
	if (mel < 0) return NUMundefined;
	return 700 * (pow (10.0, mel / 2595.0) - 1);
}

double NUMhertzToMel2 (double hz)
{
	if (hz < 0) return NUMundefined;
	return 2595 * log10 (1 + hz/700);
}

double NUMhertzToBark_traunmueller (double hz)
{
	if (hz < 0) return NUMundefined;
	return 26.81 * hz /(1960 + hz) - 0.53;
}

double NUMbarkToHertz_traunmueller (double bark)
{
	if (bark < 0 || bark > 26.28) return NUMundefined;
	return 1960* (bark + 0.53) / (26.28 - bark);
}

double NUMbarkToHertz_schroeder (double bark)
{
	return 650.0 * sinh (bark / 7.0);
}

double NUMbarkToHertz_zwickerterhardt (double hz)
{
	if (hz < 0) return NUMundefined;
	return 13 * atan (0.00076 * hz) + 3.5 * atan (hz / 7500);
}

double NUMhertzToBark_schroeder (double hz)
{
	double h650;
	if (hz < 0) return NUMundefined;
	h650 = hz / 650;
	return 7.0 * log (h650 + sqrt (1 + h650 * h650));
}

double NUMbarkToHertz2 (double bark)
{
	if (bark < 0) return NUMundefined;
	return 650.0 * sinh (bark / 7.0);
}

double NUMhertzToBark2 (double hz)
{
	double h650;
	if (hz < 0) return NUMundefined;
	h650 = hz / 650;
	return 7.0 * log (h650 + sqrt (1 + h650 * h650));
}

double NUMbladonlindblomfilter_amplitude (double zc, double z)
{
	double dz = zc - z + 0.474;
	return pow (10, 1.581 + 0.75 * dz - 1.75 * sqrt (1 + dz * dz));
}

double NUMsekeyhansonfilter_amplitude (double zc, double z)
{
	double dz = zc - z - 0.215;
	return pow (10, 0.7 - 0.75 * dz - 1.75 * sqrt (0.196 + dz * dz));
}

double NUMtriangularfilter_amplitude (double fl, double fc, double fh, double f)
{
	double a = 0;
	if (f > fl && f < fh)
	{
		a = f < fc ? (f - fl) / (fc - fl) : (fh - f) / (fh - fc);

		/* Normalize such that area under the filter is always 1. ???

		a /= 2 * (fh - fl);*/
	}
	return a;
}

double NUMformantfilter_amplitude (double fc, double bw, double f)
{
	double dq = (fc * fc - f * f) / (bw * f);
	return 1.0 / (dq * dq + 1.0);
}

/* Childers (1978), Modern Spectrum analysis, IEEE Press, 252-255) */
/* work[1..n+n+n];
b1 = & work[1];
b2 = & work[n+1];
aa = & work[n+n+1];
for (i=1; i<=n+n+n; i++) work[i]=0;
*/
int NUMburg (double x[], long n, double a[], int m, double *xms)
{
	for (long j = 1; j <= m; j++) a[j] = 0;
	
	autoNUMvector<double> b1 (1, n);
	autoNUMvector<double> b2 (1, n);
	autoNUMvector<double> aa (1, m);
	
	// (3)

	double p = 0.0;
	for (long j = 1; j <= n; j++)
	{
		p += x[j] * x[j];
	}

	*xms = p / n;
	if (*xms <= 0) return 0; // warning empty

	// (9)

	b1[1] = x[1];
	b2[n - 1] = x[n];
	for (long j = 2; j <= n - 1; j++)
	{
		b1[j] = b2[j - 1] = x[j];
	}

	for (long i = 1; i <= m; i++)
	{
		// (7)

		double num = 0.0, denum = 0.0;
		for (long j = 1; j <= n - i; j++)
		{
			num += b1[j] * b2[j];
			denum += b1[j] * b1[j] + b2[j] * b2[j];
		}

		if (denum <= 0) return 0; // warning ill-conditioned

		a[i] = 2.0 * num / denum;

		// (10)

		*xms *= 1.0 - a[i] * a[i];

		// (5)

		for (long j = 1; j <= i - 1; j++)
		{
			a[j] = aa[j] - a[i] * aa[i - j];
		}

		if (i < m)
		{

			// (8)  Watch out: i -> i+1

			for (long j = 1; j <= i; j++)
			{
				aa[j] = a[j];
			}
			for (long j = 1; j <= n - i - 1; j++)
			{
				b1[j] -= aa[i] * b2[j];
				b2[j] = b2[j + 1] - aa[i] * b1[j + 1];
			}
		}
	}
	return 1;
}

int NUMdmatrix_to_dBs (double **m, long rb, long re, long cb, long ce,
	double ref, double factor, double floor)
{
	double ref_db, factor10 = factor * 10;
	double max = m[rb][cb], min = max;

	Melder_assert (ref > 0 && factor > 0 && rb <= re && cb <= ce);

	for (long i = rb; i <= re; i++)
	{
		for (long j = cb; j <= ce; j++)
		{
			if (m[i][j] > max) max = m[i][j];
			else if (m[i][j] < min) min = m[i][j];
		}
	}

	if (max < 0 || min < 0) return Melder_error1 (L"NUMdmatrix_to_dBs: all "
		"matrix elements must be positive.");

	ref_db = factor10 * log10 (ref);

	for (long i = rb; i <= re; i++)
	{
		for (long j = cb; j <= ce; j++)
		{
			double mij = floor;
			if (m[i][j] > 0)
			{
				mij = factor10 * log10 (m[i][j]) - ref_db;
				if (mij < floor) mij = floor;
			}
			m[i][j] = mij;
		}
	}
	return 1;
}

double **NUMcosinesTable (long first, long last, long npoints)
{
	Melder_assert (0 < first && first <= last && npoints > 0);
	autoNUMmatrix<double> m (first, last, 1, npoints);
	for (long i = first; i <= last; i++)
	{
		double f = i * NUMpi / npoints;
		for (long j = 1; j <= npoints; j++)
		{
			m[i][j] = cos (f * (j - 0.5));
		}
	}
	return m.transfer();
}

void NUMspline (double x[], double y[], long n, double yp1, double ypn,
	double y2[])
{
	autoNUMvector<double> u (1, n - 1);
	
	if (yp1 > 0.99e30)
	{
		y2[1] = u[1] = 0.0;
	}
	else
	{
		y2[1] = -0.5;
		u[1] = (3.0 / (x[2] - x[1])) * ((y[2] - y[1]) / (x[2] - x[1]) - yp1);
	}
	
	for (long i = 2; i <= n - 1; i++)
	{
		double sig = (x[i] - x[i-1]) / (x[i+1] - x[i-1]);
		double p = sig * y2[i-1] + 2.0;
		y2[i] = (sig - 1.0) / p;
		u[i] = (y[i+1] - y[i]) / (x[i+1] - x[i]) - (y[i] - y[i-1]) / (x[i] - x[i-1]);
		u[i] = (6.0 * u[i] / (x[i+1] - x[i-1]) - sig * u[i-1]) / p;
	}
	
	double qn, un;
	if (ypn > 0.99e30)
	{
		qn = un = 0.0;
	}
	else
	{
		qn = 0.5;
		un = (3.0 / (x[n] - x[n-1])) * (ypn - (y[n] - y[n-1]) / (x[n] - x[n-1]));
	}

	y2[n] = (un - qn * u[n-1]) / (qn * y2[n-1] + 1.0);
	for (long k = n - 1; k >= 1; k--)
	{
		y2[k] = y2[k] * y2[k+1] + u[k];
	}
}

void NUMsplint (double xa[], double ya[], double y2a[], long n, double x, double *y)
{
	long klo = 1, khi = n;
	while (khi - klo > 1)
	{
		long k = (khi + klo) >> 1;
		if (xa[k] > x) khi = k;
		else klo = k;
	}
	double h = xa[khi] - xa[klo];
	if (h == 0.0) Melder_throw ("NUMsplint: bad input value.");
	double a = (xa[khi] - x) / h;
	double b = (x - xa[klo]) / h;
	*y = a * ya[klo] + b * ya[khi]+((a * a * a - a) * y2a[klo] +
		(b * b * b - b) * y2a[khi]) * (h * h) / 6.0;
}

double NUMsinc (const double x)
{
        struct gsl_sf_result_struct result;
        int status = gsl_sf_sinc_e (x / NUMpi, &result);
        return status == GSL_SUCCESS ? result. val : NUMundefined;
}

double NUMsincpi (const double x)
{
        struct gsl_sf_result_struct result;
        int status = gsl_sf_sinc_e (x, &result);
        return status == GSL_SUCCESS ? result. val : NUMundefined;
}

#define MACRO_NUMvector_extrema(TYPE) \
{ \
	TYPE tmin = v[lo]; \
	TYPE tmax = tmin; \
	for (long i = lo + 1; i <= hi; i++) \
	{ \
		if (v[i] < tmin) tmin = v[i]; \
		else if (v[i] > tmax) tmax = v[i]; \
	} \
	*min = tmin; *max = tmax; \
}

void NUMlvector_extrema (long v[], long lo, long hi, double *min, double *max)
MACRO_NUMvector_extrema (long)

void NUMfvector_extrema (float v[], long lo, long hi, double *min, double *max)
MACRO_NUMvector_extrema (float)

void NUMivector_extrema (int v[], long lo, long hi, double *min, double *max)
MACRO_NUMvector_extrema (int)

void NUMdvector_extrema (double v[], long lo, long hi, double *min, double *max)
MACRO_NUMvector_extrema (double)

void NUMsvector_extrema (short v[], long lo, long hi, double *min, double *max)
MACRO_NUMvector_extrema (short)

#undef MACRO_NUMvector_extrema

#define MACRO_NUMclip(TYPE)\
{\
	TYPE tmin = min, tmax = max; \
	for (long i = lo; i <= hi; i++) \
	{ \
		if (v[i] < tmin) v[i] = tmin;\
		else if (v[i] > tmax) v[i] = tmax;\
	}\
}


void NUMlvector_clip (long v[], long lo, long hi, double min, double max)
MACRO_NUMclip (long)

void NUMfvector_clip (float v[], long lo, long hi, double min, double max)
MACRO_NUMclip (float)

void NUMivector_clip (int v[], long lo, long hi, double min, double max)
MACRO_NUMclip (int)

void NUMdvector_clip (double v[], long lo, long hi, double min, double max)
MACRO_NUMclip (double)

void NUMsvector_clip (short v[], long lo, long hi, double min, double max)
MACRO_NUMclip (short)

#undef MACRO_NUMclip

#define MACRO_NUMmatrix_extrema(TYPE)\
{\
	TYPE xmin, xmax;\
	xmax = xmin = x[rb][cb];\
	for (long i = rb; i <= re; i++)\
	{\
		for (long j = cb; j <= ce; j++)\
		{\
			TYPE t = x[i][j];\
			if (t < xmin) xmin = t; \
			else if (t > xmax) xmax = t;\
		}\
	}\
	*min = xmin; *max = xmax;\
}

void NUMdmatrix_extrema (double **x, long rb, long re, long cb, long ce,
	double *min, double *max)
MACRO_NUMmatrix_extrema(double)

void NUMfmatrix_extrema (float **x, long rb, long re, long cb, long ce,
	double *min, double *max)
MACRO_NUMmatrix_extrema(float)

void NUMlmatrix_extrema (long **x, long rb, long re, long cb, long ce,
	double *min, double *max)
MACRO_NUMmatrix_extrema(long)

void NUMimatrix_extrema (int **x, long rb, long re, long cb, long ce,
	double *min, double *max)
MACRO_NUMmatrix_extrema(int)

void NUMsmatrix_extrema (short **x, long rb, long re, long cb, long ce,
	double *min, double *max)
MACRO_NUMmatrix_extrema(short)

#undef MACRO_NUMmatrix_extrema

/* Does the line segment from (x1,y1) to (x2,y2) intersect with the line segment from (x3,y3) to (x4,y4)? */
int NUMdoLineSegmentsIntersect (double x1, double y1, double x2, double y2, double x3, double y3,
	double x4, double y4)
{
	int o11 = NUMgetOrientationOfPoints (x1, y1, x2, y2, x3, y3);
	int o12 = NUMgetOrientationOfPoints (x1, y1, x2, y2, x4, y4);
	int o21 = NUMgetOrientationOfPoints (x3, y3, x4, y4, x1, y1);
	int o22 = NUMgetOrientationOfPoints (x3, y3, x4, y4, x2, y2);
	return ((o11 * o12 < 0) && (o21 * o22 < 0)) || (o11 * o12 * o21 * o22 == 0);
}

int NUMgetOrientationOfPoints (double x1, double y1, double x2, double y2, double x3, double y3)
{
	int orientation;
	double dx2 = x2 - x1, dy2 = y2 - y1;
	double dx3 = x3 - x1, dy3 = y3 - y1;
	if (dx2 * dy3 > dy2 * dx3) orientation = 1;
	else if (dx2 * dy3 < dy2 * dx3) orientation = -1;
	else
	{
		if ((dx2 * dx3 < 0) || (dy2 * dy3 < 0)) orientation = -1;
		else if ((dx2*dx2+dy2*dy2) >= (dx3*dx3+dy3*dy3)) orientation = 0;
		else orientation = 1;
	}
	return orientation;
}

int NUMgetIntersectionsWithRectangle (double x1, double y1, double x2, double y2,
	double xmin, double ymin, double xmax, double ymax, double *xi, double *yi)
{
	double x[6], y[6], t;
	long ni = 0;

	x[1] = x[4] = x[5] = xmin;
	x[2] = x[3] = xmax;
	y[1] = y[2] = y[5] = ymin;
	y[3] = y[4] = ymax;
/*
	Calculate intersection of line segment through p1=(x1,y1) to p2(x2,y2) with line segment
	through p3=(x3,y3) to p4=(x4,y4).
	Parametrisation of the lines:
	l1 = p1 + s (p2 - p1), s in (-inf,+inf)
	l2 = p3 + t (p4 - p3), t in (-inf,+inf).
	When s and t are in [0,1] we have line segments between the points.
	At the intersection l1 == l2. We get for the x and y coordinates:
		x1 + s (x2 - x1) = x3 + t (x4 - x3).............(1)
		y1 + s (y2 - y1) = y3 + t (y4 - y3).............(2)
	Multiply (1)*(y2 - y1) and (2)*(x2 - x1):
		x1 (y2 - y1) + s (x2 - x1)(y2 - y1) = x3 (y2 - y1) + t (x4 - x3)(y2 - y1).......(3)
		y1 (x2 - x1) + s (y2 - y1)(x2 - x1) = y3 (x2 - x1) + t (y4 - y3)(x2 - x1).......(4)
	(3)-(4) with y21 = y2 -y1, x21 = x2 - x1, x43 = x4 - x3, ...
		x1 y21 - y1 x21 = x3 y21 - y3 x21 +t (x43 y21 - y43 x21)
	Combining:
		y31 x21 - x31 y21 = t (x43 y21 - y43 x21)
	Therefor at the intersection we have:

		t = (y31 x21 - x31 y21) / (x43 y21 - y43 x21)

	If (x43 y21 - y43 x21) == 0
		There is no intersection.
	If (t < 0 || t >= 1)
		No intersection in the segment l2
		To count intersections in a corner only once we have t < 0 instead of t <= 0!
*/

	for (long i = 1; i <= 4; i++)
	{
		double denom = (x[i+1] - x[i]) * (y2 - y1) - (y[i+1] - y[i]) * (x2 - x1);
		if (denom == 0) continue;
		/* We have an intersection. */
		t = ((y[i] - y1) * (x2 - x1) - (x[i] - x1) * (y2 - y1)) / denom;
		if (t < 0 || t >= 1) continue;
		/* Intersection is within rectangle side. */
		ni++;
		if (ni > 2)
		{
			(void) Melder_error1 (L"Too many intersections.");
			return 2;
		}
		xi[ni] = x[i] + t * (x[i+1] - x[i]);
		yi[ni] = y[i] + t * (y[i+1] - y[i]);
	}
	return ni;
}


int NUMclipLineWithinRectangle (double xl1, double yl1, double xl2, double yl2, double xr1, double yr1,
	double xr2, double yr2, double *xo1, double *yo1, double *xo2, double *yo2)
{
	int hline, vline, ncrossings = 0, xswap, yswap;
	double a, b, x, y, t, xc[5], yc[5], xmin, xmax, ymin, ymax;

	*xo1 = xl1; *yo1 = yl1; *xo2 = xl2; *yo2 = yl2;

	// This test first because we expect the majority of the tested segments to be
	// within the rectangle
	if (xl1 >= xr1 && xl1 <= xr2 && yl1 >= yr1 && yl1 <= yr2 &&
		xl2 >= xr1 && xl2 <= xr2 && yl2 >= yr1 && yl2 <= yr2) return 1;

	// All lines that are completely outside the rectangle
	if ((xl1 <= xr1 && xl2 <= xr1) || (xl1 >= xr2 && xl2 >= xr2) ||
		(yl1 <= yr1 && yl2 <= yr1) || (yl1 >= yr2 && yl2 >= yr2)) return 0;

	// At least line spans (part of) the rectangle.
	// Get extremes in x and y of the line for easy testing further on.
	if (xl1 < xl2)
	{
		xmin = xl1; xmax = xl2; xswap = 0;
	}
	else
	{
		xmin = xl2; xmax = xl1; xswap = 1;
	}
	if (yl1 < yl2)
	{
		ymin = yl1; ymax = yl2; yswap = 0;
	}
	else
	{
		ymin = yl2; ymax = yl1; yswap = 1;
	}

	if ((hline = (yl1 == yl2)) == true)
	{
		if (xmin < xr1) *xo1 = xr1;
		if (xmax > xr2) *xo2 = xr2;
		if (xswap)
		{
			t = *xo1; *xo1 = *xo2; *xo2 = t;
		}
		return 1;
	}
	if ((vline = (xl1 == xl2)) == true)
	{
		if (ymin < yr1) *yo1 = yr1;
		if (ymax > yr2) *yo2 = yr2;
		if (yswap)
		{
			t = *yo1; *yo1 = *yo2; *yo2 = t;
		}
		return 1;
	}

	// Now we know that the line from (x1,y1) to (x2,y2) is neither horizontal nor vertical.
	// Parametrize it as y = ax + b

	a = (yl1 -yl2) / (xl1 - xl2);
	b = yl1 - a * xl1;


	//	To determine the crossings we have to avoid counting the crossings in a corner twice.
	//	Therefore we test the corners inclusive (..<=..<=..) on the vertical borders of the rectangle
	//	and exclusive (..<..<) at the horizontal borders.


	y = a * xr1 + b; // Crossing at y with left border: x = xr1

	if (y >= yr1 && y <= yr2 && xmin < xr1) // Within vertical range?
	{
		ncrossings++;
		xc[ncrossings] = xr1; yc[ncrossings] = y;
		xc[2] = xmax;
		yc[2] = xl1 > xl2 ? yl1 : yl2;
	}

	x = (yr2 - b) / a; // Crossing at x with top border: y = yr2

	if (x > xr1 && x < xr2 && ymax > yr2) // Within horizontal range?
	{
		ncrossings++;
		xc[ncrossings] = x; yc[ncrossings] = yr2;
		if (ncrossings == 1)
		{
			yc[2] = ymin;
			xc[2] = yl1 < yl2 ? xl1 : xl2;
		}
	}

	y = a * xr2 + b; // Crossing at y with right border: x = xr2

	if (y >= yr1 && y <= yr2 && xmax > xr2) // Within vertical range?
	{
		ncrossings++;
		xc[ncrossings] = xr2; yc[ncrossings] = y;
		if (ncrossings == 1)
		{
			xc[2] = xmin;
			yc[2] = xl1 < xl2 ? yl1 : yl2;
		}
	}

	x = (yr1 - b) / a; // Crossing at x with bottom border: y = yr1

	if (x > xr1 && x < xr2 && ymin < yr1)
	{
		ncrossings++;
		xc[ncrossings] = x; yc[ncrossings] = yr1;
		if (ncrossings == 1)
		{
			yc[2] = ymax;
			xc[2] = yl1 > yl2 ? xl1 : xl2;
		}
	}
	if (ncrossings == 0)
	{
		return 0;
	}
	if (ncrossings == 1 || ncrossings == 2)
	{
		// if start and endpoint of line are outside rectangle and ncrossings == 1,
		// than the line only touches.
		if (ncrossings == 1 &&
			(xl1 < xr1 || xl1 > xr2 || yl1 < yr1 || yl1 > yr2) &&
			(xl2 < xr1 || xl2 > xr2 || yl2 < yr1 || yl2 > yr2)) return 0;

		if ((xc[1] > xc[2] && ! xswap) || (xc[1] < xc[2] && xswap))
		{
			t = xc[1]; xc[1] = xc[2]; xc[2] = t;
			t = yc[1]; yc[1] = yc[2]; yc[2] = t;
		}
		*xo1 = xc[1]; *yo1 = yc[1]; *xo2 = xc[2]; *yo2 = yc[2];
	}
	else
	{
		return Melder_error1 (L"Too many crossings found.");
	}
	return 1;
}

void NUMgetEllipseBoundingBox (double a, double b, double cospsi, double *width, double *height)
{

	Melder_assert (cospsi>= -1 && cospsi <= 1);

	if (cospsi == 1)
	{
		/*
			a-axis along x-axis
		*/

		*width = a;
		*height = b;
	}
	else if (cospsi == 0)
	{
		/*
			a-axis along y-axis
		*/

		*width = b;
		*height = a;
	}
	else
	{
		double psi = acos (cospsi), sn = sin (psi);
		double phi = atan2 (-b * sn, a * cospsi);
		*width = fabs (a * cospsi * cos (phi) - b * sn * sin (phi));
		phi = atan2 (b *cospsi , a * sn);
		*height = fabs (a * sn * cos (phi) + b * cospsi * sin (phi));
	}
}

/*
Closely modelled after the netlib code by Oleg Keselyov.
*/
double NUMminimize_brent (double (*f) (double x, void *closure), double a, double b,
	void *closure, double tol, double *fx)
{
	double x, v, fv, w, fw;
	const double golden = 1 - NUM_goldenSection;
	const double sqrt_epsilon = sqrt (NUMfpp -> eps);
	long itermax = 60;

	Melder_assert (tol > 0 && a < b);

	/* First step - golden section */

	v = a + golden * (b - a);
	fv = (*f)(v, closure);
	x = v;  w = v;
	*fx = fv;  fw = fv;

	for(long iter = 1; iter <= itermax; iter++)
	{
		double range = b - a;
		double middle_range = (a + b) / 2;
		double tol_act = sqrt_epsilon * fabs(x) + tol / 3;
		double new_step; /* Step at this iteration */



		if (fabs(x - middle_range) + range / 2 <= 2 * tol_act)
		{
      		return x;
		}

		/* Obtain the golden section step */

		new_step = golden * (x < middle_range ? b - x : a - x);


		/* Decide if the parabolic interpolation can be tried	*/

		if (fabs(x - w) >= tol_act)
		{
			/*
				Interpolation step is calculated as p/q;
				division operation is delayed until last moment.
			*/

			double p, q, t;

			t = (x - w) * (*fx - fv);
			q = (x - v) * (*fx - fw);
			p = (x - v) * q - (x - w) * t;
			q = 2 * (q - t);

			if( q > 0)
			{
				p = -p;
			}
			else
			{
				q = -q;
			}

			/*
				If x+p/q falls in [a,b], not too close to a and b,
				and isn't too large, it is accepted.
				If p/q is too large then the golden section procedure can
				reduce [a,b] range.
			*/

			if( fabs (p) < fabs (new_step * q) &&
				p > q * (a - x + 2 * tol_act) &&
				p < q * (b - x - 2 * tol_act))
			{
				new_step = p / q;
			}
		}

		/* Adjust the step to be not less than tolerance. */

		if (fabs(new_step) < tol_act)
		{
			new_step = new_step > 0 ? tol_act : - tol_act;
		}

		/* Obtain the next approximation to min	and reduce the enveloping range */

		{
			double t = x + new_step;	/* Tentative point for the min	*/
			double ft = (*f)(t, closure);

			/*
				If t is a better approximation, reduce the range so that
				t would fall within it. If x remains the best, reduce the range
				so that x falls within it.
			*/

			if (ft <= *fx)
			{
				if( t < x )
				{
					b = x;
				}
				else
				{
					a = x;
				}

				v = w;  w = x;  x = t;
				fv = fw;  fw = *fx;  *fx = ft;
			}
			else
			{
				if (t < x)
				{
					a = t;
				}
				else
				{
					b = t;
				}

				if (ft <= fw || w == x)
				{
					v = w; w = t;
					fv = fw; fw = ft;
				}
				else if (ft <= fv || v == x || v == w)
				{
					v = t;
					fv=ft;
				}
			}
		}
	}
	Melder_warning3 (L"NUMminimize_brent: maximum number of iterations (", Melder_integer (itermax), L") exceeded.");
	return x;
}

/*
	probs is probability vector, i.e. all 0 <= probs[i] <= 1 and sum(i=1;i=nprobs, probs[i])= 1
	p is a probability
*/
long NUMgetIndexFromProbability (double *probs, long nprobs, double p)
{
	long index = 1;
	double psum = probs[index];
	while (p > psum && index < nprobs)
	{
		psum += probs[++index];
	}
	return index;
}

#undef MAX
#undef MIN
#undef SIGN

/* End of file NUM2.c */
