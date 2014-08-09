/*
	Ordo is program for calculating ratings of engine or chess players
    Copyright 2013 Miguel A. Ballicora

    This file is part of Ordo.

    Ordo is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Ordo is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Ordo.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "indiv.h"
#include "ordolim.h"




static double
fxpect (double a, double b, double beta)
{
	return 1.0 / (1.0 + exp((b-a)*beta));
}

#define DRAWRATE_AT_EQUAL_STRENGTH 0.33
#define DRAWFACTOR (1/(2*(DRAWRATE_AT_EQUAL_STRENGTH))-0.5)

static void
fget_pWDL(double dr /*delta rating*/, double *pw, double *pd, double *pl, double beta)
{
	// Performance comprises wins and draws.
	// if f is expected performance from 0 to 1.0, then
	// f = pwin + pdraw/2
	// from that, dc is the fraction of points that come from draws, not wins, so
	// pdraw (probability of draw) = 2 * f * dc
	// calculation of dc is an empirical formula to fit average data from CCRL:
	// Draw rate of equal engines is near 0.33, and decays on uneven matches.

	double f;
	double pdra, pwin, plos;
// 	double dc; 
	bool_t switched;
	
	switched = dr < 0;

	if (switched) dr = -dr;

	f = fxpect (dr,0,beta);

#if 0
	dc = 0.5 / (0.5 + DRAWFACTOR * exp(dr*BETA));
	pdra = 2 * f * dc;
	pwin = f - pdra/2;
	plos = 1 - pwin - pdra; 
#else
	pwin = f * f;
	plos = (1-f) * (1-f);
	pdra = 1 - pwin - plos;
#endif

	if (switched) {
		*pw = plos;
		*pd = pdra;
		*pl = pwin;
	} else {
		*pw = pwin;
		*pd = pdra;
		*pl = plos;
	}
	return;
}

//===============================================================

#if 1
static double
ind_expected (double x, double *rtng, double *weig, int n, double beta)
{
	int i;
	double cume = 0;
	for (i = 0; i < n; i++) {
		cume += weig[i] * fxpect (x, rtng[i], beta);
	}
	return cume;
}

static double 
adjust_x (double x, double xp, double sc, double delta, double kappa)
{
	double y;
	double	d;
	d = xp - sc;
	
	d = d < 0? -d: d;
	y = d / (kappa+d);
	if (xp > sc) {
		x -= delta * y;
	} else {
		x += delta * y;
	}
	return x;	
}


static double
calc_ind_rating(double cume_score, double *rtng, double *weig, int r, double beta)
{
	int 	i;
	double 	olddev, curdev;
	int		rounds = 10000;
	double 	delta = 200.0;
	double 	kappa = 0.05;
	double 	denom = 2;
	int 	phase = 0;
	int 	n = 20;

	double  D,sc,oldx;

	double x = 2000;
	double xp;

	D = cume_score - ind_expected(x,rtng,weig,r,beta) ;
	curdev = D*D;
	olddev = curdev;

	while (n-->0) {
		double kk = 1.0;
		for (i = 0; i < rounds; i++) {
			oldx = x;
			olddev = curdev;

			sc = cume_score;
			xp = ind_expected(x,rtng,weig,r,beta);
			x  = adjust_x (x, xp, sc, delta, kappa*kk);
			xp = ind_expected(x,rtng,weig,r,beta);
			D = xp - sc;
			curdev = D*D;

			if (curdev >= olddev) {
				x = oldx;
				D = cume_score - ind_expected(x,rtng,weig,r,beta) ;
				curdev = D*D;	
				assert (curdev == olddev);
				break;
			};	

			if (curdev < 0.000001) break;
			kk *= 0.995;
		}

		delta /= denom;
		kappa *= denom;

		phase++;

		if (curdev < 0.000001) break;
	}

	return x;
}
#endif

//=========================================


static double
prob2absolute_result (int perftype, double myrating, double *rtng, double *weig, int n, double beta)
{
	int i;
	double p, cume;
	double pwin, pdraw, ploss;
	assert(n);
	assert(perftype == PERF_SUPERWINNER || perftype == PERF_SUPERLOSER);

	cume = 1.0;
	if (PERF_SUPERWINNER == perftype) {
		for (i = 0; i < n; i++) {
			fget_pWDL(myrating - rtng[i], &pwin, &pdraw, &ploss, beta);
			p = pwin;
			cume *= exp(weig[i] * log (p)); // p ^ weight
		}	
	} else {
		for (i = 0; i < n; i++) {
			fget_pWDL(myrating - rtng[i], &pwin, &pdraw, &ploss, beta);
			p = ploss;
			cume *= exp(weig[i] * log (p)); // p ^ weight
		}
	}
	return cume;
}


static double
calc_ind_rating_superplayer (int perf_type, double x_estimated, double *rtng, double *weig, int r, double beta)
{
	int 	i;
	double 	old_unfit, cur_unfit;
	int		rounds = 2000;
	double 	delta = 200.0;
	double 	denom = 2;
	double fdelta;
	double  D, oldx;

	double x = x_estimated;

	if (perf_type == PERF_SUPERLOSER) 
		D = - 0.5 + prob2absolute_result(perf_type, x, rtng, weig, r, beta);		
	else
		D = + 0.5 - prob2absolute_result(perf_type, x, rtng, weig, r, beta);

	cur_unfit = D * D;
	old_unfit = cur_unfit;

	fdelta = D < 0? -delta:  delta;	

	for (i = 0; i < rounds; i++) {

		oldx = x;
		old_unfit = cur_unfit;

		x += fdelta;

		if (perf_type == PERF_SUPERLOSER) 
			D = - 0.5 + prob2absolute_result(perf_type, x, rtng, weig, r, beta);		
		else
			D = + 0.5 - prob2absolute_result(perf_type, x, rtng, weig, r, beta);

		cur_unfit = D * D;
		fdelta = D < 0? -delta: delta;


		if (cur_unfit >= old_unfit) {
			x = oldx;
			cur_unfit = old_unfit;
			delta /= denom;

		} else {

		}	

		if (cur_unfit < 0.0000000001) break;
	}

	return x;
}

static double calc_ind_rating(double cume_score, double *rtng, double *weig, int r, double beta);
static double calc_ind_rating_superplayer (int perf_type, double x_estimated, double *rtng, double *weig, int r, double beta);

void
rate_super_players(bool_t quiet, struct ENC *enc, int N_enc, int *performance_type, int n_players, double *ratingof, double white_advantage, bool_t *flagged,
char *Name[], double beta)
{
	int j, e;
	int myenc_n = 0;
	static struct ENC myenc[MAXENCOUNTERS];

	for (j = 0; j < n_players; j++) {

		if (performance_type[j] != PERF_SUPERWINNER && performance_type[j] != PERF_SUPERLOSER) 
			continue;

		myenc_n = 0; // reset

		if (performance_type[j] == PERF_SUPERWINNER)
			if (!quiet) printf ("  all wins   --> %s\n", Name[j]);

		if (performance_type[j] == PERF_SUPERLOSER) 
			if (!quiet) printf ("  all losses --> %s\n", Name[j]);

		for (e = 0; e < N_enc; e++) {
			int w = enc[e].wh;
			int b = enc[e].bl;
			if (j == w /*&& performance_type[b] == PERF_NORMAL*/) {
				myenc[myenc_n++] = enc[e];
			} else
			if (j == b /*&& performance_type[w] == PERF_NORMAL*/) {
				myenc[myenc_n++] = enc[e];
			}
		}

		{
			double	cume_score = 0; 
			double	cume_total = 0;
			static double weig[MAXPLAYERS];
			static double rtng[MAXPLAYERS];
			int		r = 0;
 	
			while (myenc_n-->0) {
				int n = myenc_n;
				if (myenc[n].wh == j) {
					int opp = myenc[n].bl;
					weig[r	] = myenc[n].played;
					rtng[r++] = ratingof[opp] - white_advantage;
					cume_score += myenc[n].wscore;
					cume_total += myenc[n].played;
			 	} else 
				if (myenc[myenc_n].bl == j) {
					int opp = myenc[n].wh;
					weig[r	] = myenc[n].played;
					rtng[r++] = ratingof[opp] + white_advantage;
					cume_score += myenc[n].played - myenc[n].wscore;
					cume_total += myenc[n].played;
				} else {
					fprintf(stderr,"ERROR!! function rate_super_players()\n");
					exit(0);
					continue;
				} 
			}

			if (performance_type[j] == PERF_SUPERWINNER) {
				double ori_estimation = calc_ind_rating (cume_score-0.25, rtng, weig, r, beta); 
				ratingof[j] = calc_ind_rating_superplayer (PERF_SUPERWINNER, ori_estimation, rtng, weig, r, beta);
			}
			if (performance_type[j] == PERF_SUPERLOSER) {
				double ori_estimation = calc_ind_rating (cume_score+0.25, rtng, weig, r, beta); 
				ratingof[j] = calc_ind_rating_superplayer (PERF_SUPERLOSER,  ori_estimation, rtng, weig, r, beta);
			}
			flagged[j] = FALSE;

		}
	}

	return;
}


