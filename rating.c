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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "rating.h"
#include "encount.h"
#include "indiv.h"
#include "xpect.h"

#include "datatype.h"

#if 1
#define CALCIND_SWSL
#endif

static double adjust_drawrate (double start_wadv, double *ratingof, int N_enc, const struct ENC *enc, double beta);

// no globals
static void
ratings_restore (int n_players, const double *r_bk, double *r_of)
{
	int j;
	for (j = 0; j < n_players; j++) {
		r_of[j] = r_bk[j];
	}	
}

// no globals
static void
ratings_backup (int n_players, const double *r_of, double *r_bk)
{
	int j;
	for (j = 0; j < n_players; j++) {
		r_bk[j] = r_of[j];
	}	
}

// no globals
static double 
deviation (int n_players, const bool_t *flagged, const double *expected, const double *obtained, const int *playedby)
{
	double accum = 0;
	double diff;
	int j;

	for (accum = 0, j = 0; j < n_players; j++) {
		if (!flagged[j]) {
			diff = expected[j] - obtained [j];
			accum += diff * diff / playedby[j];
		}
	}		
	return accum;
}

// no globals
static double
adjust_rating 	( double delta
				, double kappa
				, int n_players
				, const bool_t *flagged
				, const bool_t *prefed
				, const double *expected 
				, const double *obtained 
				, int *playedby
				, double general_average
				, bool_t multiple_anchors_present
				, bool_t anchor_use
				, int anchor
				, double *ratingof
)
{
	int 	j, notflagged;
	double 	d, excess, average;
	double 	y = 1.0;
	double 	ymax = 0;
	double 	accum = 0;

	/*
	|	1) delta and 2) kappa control convergence speed:
	|	Delta is the standard increase/decrease for each player
	|	But, not every player gets that "delta" since it is modified by
	|	by multiplier "y". The bigger the difference between the expected 
	|	performance and the observed, the bigger the "y". However, this
	|	is controled so y won't be higher than 1.0. It will be asymptotic
	|	to 1.0, and the parameter that controls how fast this saturation is 
	|	reached is "kappa". Smaller kappas will allow to reach 1.0 faster.	
	|
	|	Uses globals:
	|	arrays:	Flagged, Prefed, Expected, Obtained, Playedby, Ratingof
	|	variables: N_players, General_average
	|	flags:	Multiple_anchors_present, Anchor_use
	*/

	for (j = 0; j < n_players; j++) {
		if (	flagged[j]	// player previously removed
			|| 	prefed[j]	// already set, one of the multiple anchors
		) continue; 

		// find multiplier "y"
		d = (expected[j] - obtained[j]) / playedby[j];
		d = d < 0? -d: d;
		y = d / (kappa + d);
		if (y > ymax) ymax = y;

		// execute adjustment
		if (expected[j] > obtained [j]) {
			ratingof[j] -= delta * y;
		} else {
			ratingof[j] += delta * y;
		}
	}

	// Normalization to a common reference (Global --> General_average)
	// The average could be normalized, or the rating of an anchor.
	// Skip in case of multiple anchors present

	if (!multiple_anchors_present) {
		if (anchor_use) {
			excess  = ratingof[anchor] - general_average;
		} else {
			for (notflagged = 0, accum = 0, j = 0; j < n_players; j++) {
				if (!flagged[j]) {
					notflagged++;
					accum += ratingof[j];
				}
			}
			average = accum / notflagged;
			excess  = average - general_average;
		}
		for (j = 0; j < n_players; j++) {
			if (!flagged[j] && !prefed[j]) ratingof[j] -= excess;
		}	
	}	

	// Return maximum increase/decrease ==> "resolution"

	return ymax * delta;
}

// no globals
static void
adjust_rating_byanchor (bool_t anchor_use, int anchor, double general_average, int n_players
						, const bool_t *flagged, const bool_t *prefed, double *ratingof)
{
	double excess;
	int j;
	if (anchor_use) {
		excess  = ratingof[anchor] - general_average;	
		for (j = 0; j < n_players; j++) {
			if (!flagged[j] && !prefed[j]) ratingof[j] -= excess;
		}	
	}
}



/*
static double
overallerror_fwadv (double wadv)
{
	int i;
	double e, e2, f, rw, rb;
	double s[3];

	s[WHITE_WIN] = 1.0;
	s[BLACK_WIN] = 0.0;
	s[RESULT_DRAW] = 0.5;

	assert (WHITE_WIN < 3);
	assert (BLACK_WIN < 3);
	assert (RESULT_DRAW < 3);
	assert (DISCARD > 2);

	e2 = 0;

	for (i = 0; i < N_games; i++) {

		if (Score[i] > 2) continue;

		rw = Ratingof[Whiteplayer[i]];
		rb = Ratingof[Blackplayer[i]];

		f = xpect (rw + wadv, rb);

		e   = f - s[Score[i]];
		e2 += e * e;
	}

	return e2;
}
*/

#if 0
static double
overallerrorE_fwadv (int N_enc, const struct ENC *enc, double *ratingof, double beta, double wadv)
{
	int e, w, b;
	double dp2, f;
	double wperf ,dp;
	double fobs;

	dp2 = 0;
	for (e = 0; e < N_enc; e++) {
		w = enc[e].wh;
		b = enc[e].bl;
		f = xpect (ratingof[w] + wadv, ratingof[b], beta);
		wperf = enc[e].played * f;
		dp = wperf - enc[e].wscore; 
		dp2 += dp * dp;
	}

	return dp2;
}
#else
static double
overallerrorE_fwadv (int N_enc, const struct ENC *enc, double *ratingof, double beta, double wadv)
{
	int e, w, b;
	double dp2, f;

	dp2 = 0;
	for (e = 0; e < N_enc; e++) {
		w = enc[e].wh;
		b = enc[e].bl;
		f = xpect (ratingof[w] + wadv, ratingof[b], beta);

dp2 +=	enc[e].W * (1.0 - f) * (1.0 - f)
+		enc[e].D * (0.5 - f) * (0.5 - f)
+		enc[e].L * (0.0 - f) * (0.0 - f)
;

	}

	return dp2;
}
#endif

#define START_DELTA 100

static double
adjust_wadv (double start_wadv, double *ratingof, int N_enc, const struct ENC *enc, double beta, double start_delta)
{
	double delta, wa, ei, ej, ek;

	delta = start_delta;
	wa = start_wadv;

	do {	

#if 0
		ei = overallerror_fwadv (wa - delta);
		ej = overallerror_fwadv (wa);
		ek = overallerror_fwadv (wa + delta);
#else
		ei = overallerrorE_fwadv (N_enc, enc, ratingof, beta, wa - delta);
		ej = overallerrorE_fwadv (N_enc, enc, ratingof, beta, wa + 0    );     
		ek = overallerrorE_fwadv (N_enc, enc, ratingof, beta, wa + delta);
#endif
		if (ei >= ej && ej <= ek) {
			delta = delta / 2;
		} else
		if (ej >= ei && ei <= ek) {
			wa -= delta;
		} else
		if (ei >= ek && ek <= ej) {
			wa += delta;
		}

	} while (
		delta > 0.01 
		&& -1000 < wa && wa < 1000
	);
	
	return wa;
}


//============ center adjustment begin

static void ratings_copy (const double *r, long n, double *t) {long i;	for (i = 0; i < n; i++) {t[i] = r[i];}}

static double 
unfitness		( const struct ENC *enc
				, int 			n_enc
				, int			n_players
				, const double *ratingof
				, const bool_t *flagged
				, double		white_adv
				, double		beta

				, double *		obtained
				, double *		expected
				, int *			playedby
)
{
		double dev;
		calc_expected (enc, n_enc, white_adv, n_players, ratingof, expected, beta);
		dev = deviation (n_players, flagged, expected, obtained, playedby);
		return dev;
}

static void
mobile_center_apply_excess (double excess, long n_players, const bool_t *flagged, const bool_t *prefed, double *ratingof)
{
	long j;
	for (j = 0; j < n_players; j++) {
		if (!flagged[j] && !prefed[j]) {
			ratingof[j] += excess;
		}
	}	
	return;
}

static double
unfitness_fcenter 	( double excess
					, const struct ENC *enc
					, int 			n_enc
					, int			n_players
					, const double *ratingof
					, const bool_t *flagged
					, const bool_t *prefed
					, double		white_adv
					, double		beta

					, double *		obtained
					, double *		expected
					, int *			playedby
					, double 	   *ratingtmp)
{
	double u;
	ratings_copy (ratingof, n_players, ratingtmp);
	mobile_center_apply_excess (excess, n_players, flagged, prefed, ratingtmp);
	u = unfitness	( enc
					, n_enc
					, n_players
					, ratingtmp
					, flagged
					, white_adv
					, beta
					, obtained
					, expected
					, playedby);
	return u;
}

#define MIN_DEVIA 0.000000001
#define MIN_RESOL 0.000001

static double absol(double x) {return x >= 0? x: -x;}

#include "fit1d.h"

struct UNFITPAR {
	const struct ENC *	enc;
	int 				n_enc;
	int					n_players;
	const double *		ratingof;
	const bool_t *		flagged;
	const bool_t *		prefed;
	double				white_adv;
	double				beta;
	double *			obtained;
	double *			expected;
	int *				playedby;
	double *			ratingtmp;
};

static double
unfitf (double x, const void *p)
{
	const struct UNFITPAR *q = p;
	return
	unfitness_fcenter 	(x
						, q->enc, q->n_enc, q->n_players, q->ratingof, q->flagged, q->prefed
						, q->white_adv, q->beta, q->obtained, q->expected, q->playedby, q->ratingtmp);
}

static double
optimum_centerdelta	( double 			start_delta
					, double 			end_delta
					, const struct ENC *enc
					, int 				n_enc
					, int				n_players
					, const double *	ratingof
					, const bool_t *	flagged
					, const bool_t *	prefed
					, double			white_adv
					, double			beta
					, double *			obtained
					, double *			expected
					, int *				playedby
					, double *			ratingtmp
					)
{
	struct UNFITPAR p;
	p.enc 		= enc;
	p.n_enc	= n_enc;
	p.n_players= n_players;
	p.ratingof	= ratingof;
	p.flagged	= flagged;
	p.prefed	= prefed;
	p.white_adv= white_adv;
	p.beta		= beta;
	p.obtained	= obtained;
	p.expected	= expected;
	p.playedby	= playedby;
	p.ratingtmp= ratingtmp;

	return quadfit1d (end_delta, -start_delta, start_delta, unfitf, &p);
}
//============ center adjustment end




int
calc_rating2 	( bool_t 		quiet
				, struct ENC *	enc
				, int 			N_enc

				, int			N_players
				, double *		Obtained

				, int *			Playedby
				, double *		Ratingof
				, double *		Ratingbk
				, int *			Performance_type

				, bool_t *		Flagged
				, bool_t *		Prefed

				, double		*pWhite_advantage
				, double		General_average

				, bool_t		Multiple_anchors_present
				, bool_t		Anchor_use
				, int			Anchor
				
				, int			N_games
				, int *			Score
				, int *			Whiteplayer
				, int *			Blackplayer

				, char *		Name[]
				, double		BETA
//
				, bool_t 		adjust_white_advantage

				, bool_t		adjust_draw_rate
				, double		*pDraw_date
)
{

static double ratingtmp[MAXPLAYERS]; //FIXME bad for SMP

	double 	olddev, curdev, outputdev;
	int 	i;
	int		rounds = 10000;
	double 	delta = 200.0;
	double 	kappa = 0.05;
	double 	denom = 2;
	int 	phase = 0;
	int 	n = 20;
	double 	resol;
	bool_t	doneonce = FALSE;

	int times_ori = 10;
	int times;

	double white_adv = *pWhite_advantage;
	double wa_previous = *pWhite_advantage;
	double wa_progress = START_DELTA;
	double min_devia = MIN_DEVIA;
	double min_resol = MIN_RESOL;

	double draw_rate = *pDraw_date;

	double *expected = NULL;
	size_t allocsize;

	allocsize = sizeof(double) * (size_t)(N_players+1);
	expected = malloc(allocsize);
	if (expected == NULL) {
		fprintf(stderr, "Not enough memory to allocate all players\n");
		exit(EXIT_FAILURE);
	}


	if (!adjust_white_advantage) times_ori = 1;
	times = times_ori;

	while (times-->0 && wa_progress > 0.01) {

		rounds = 10000;
		delta = 200.0;
		kappa = 0.05;
		denom = 2;
		phase = 0;
		n = 20;

		if (times == 9) {
			min_resol = 10;
		} else if (times == 8) {
			min_resol = 0.1;
		} else {
			min_resol = MIN_RESOL;
		}

		doneonce = FALSE;

		calc_obtained_playedby(enc, N_enc, N_players, Obtained, Playedby);
		calc_expected(enc, N_enc, white_adv, N_players, Ratingof, expected, BETA);

		olddev = curdev = deviation(N_players, Flagged, expected, Obtained, Playedby);

		if (!quiet) printf ("\nConvergence rating calculation (cycle #%d)\n\n", times_ori-times);
		if (!quiet) printf ("%3s %4s %12s%14s\n", "phase", "iteration", "deviation","resolution");

		while (n-->0) {
			double kk = 1.0;

double cd = 400;

			for (i = 0; i < rounds; i++) {
				bool_t failed = FALSE;
				bool_t changed;

				ratings_backup(N_players, Ratingof, Ratingbk);
				olddev = curdev;

				resol = adjust_rating 	
					( delta
					, kappa*kk
					, N_players
					, Flagged
					, Prefed
					, expected 
					, Obtained 
					, Playedby
					, General_average
					, Multiple_anchors_present
					, Anchor_use
					, Anchor
					, Ratingof
				);

				calc_expected(enc, N_enc, white_adv, N_players, Ratingof, expected, BETA);
				curdev = deviation(N_players, Flagged, expected, Obtained, Playedby);

				if (curdev >= olddev) {
					ratings_restore(N_players, Ratingbk, Ratingof);
					calc_expected(enc, N_enc, white_adv, N_players, Ratingof, expected, BETA);
					curdev = deviation(N_players, Flagged, expected, Obtained, Playedby);	
					assert (curdev == olddev);
					failed = TRUE;
				};	

{int zz = 1;
while (zz-->0)
cd = optimum_centerdelta	
					( 100.0
					, min_resol //kk*delta/1000
					, enc
					, N_enc
					, N_players
					, Ratingof
					, Flagged
					, Prefed
					, white_adv
					, BETA
					, Obtained
					, expected
					, Playedby
					, ratingtmp
					);
}

changed = absol(cd) > MIN_RESOL;

if (changed) {
	mobile_center_apply_excess (cd, N_players, Flagged, Prefed, Ratingof);
}
				failed = failed && !changed;

				if (failed) break;

				outputdev = 1000*sqrt(curdev/N_games);
				if (outputdev < min_devia || (resol+cd) < min_resol) {
					failed = TRUE;
				}

				if (failed) break;

				kk *= 0.995;

			}

			delta /= denom;
			kappa *= denom;
			outputdev = 1000*sqrt(curdev/N_games);

			if (!quiet) {
				printf ("%3d %7d %16.9f", phase, i, outputdev);
				printf ("%14.5f",resol);
				printf ("\n");
			}
			phase++;

			if (outputdev < min_devia || resol < min_resol) {
				break;
			}
		}

		if (!quiet) printf ("done\n");

		if (adjust_white_advantage) {
				white_adv = adjust_wadv (white_adv, Ratingof, N_enc, enc, BETA, doneonce? resol: START_DELTA);
				doneonce = TRUE;
				wa_progress = wa_previous > white_adv? wa_previous - white_adv: white_adv - wa_previous;
				wa_previous = white_adv;
				if (!quiet)
					printf ("Adjusted White Advantage = %.1f\n",white_adv);
		}

		if (adjust_draw_rate) {
				draw_rate = adjust_drawrate (white_adv, Ratingof, N_enc, enc, BETA);
				if (!quiet)
					printf ("Adjusted Draw Rate = %.1f %s\n\n", 100*draw_rate, "%");
		} 

		#ifdef CALCIND_SWSL
		if (!quiet) 
			printf ("Post-Convergence rating estimation\n");

		N_enc = calc_encounters(ENCOUNTERS_FULL, N_games, Score, Flagged, Whiteplayer, Blackplayer, enc);
		calc_obtained_playedby(enc, N_enc, N_players, Obtained, Playedby);
		rate_super_players(quiet, enc, N_enc, Performance_type, N_players, Ratingof, white_adv, Flagged, Name, draw_rate, BETA);
		N_enc = calc_encounters(ENCOUNTERS_NOFLAGGED, N_games, Score, Flagged, Whiteplayer, Blackplayer, enc);
		calc_obtained_playedby(enc, N_enc, N_players, Obtained, Playedby);

		if (!quiet) 
			printf ("done\n");
		#endif

		if (!Multiple_anchors_present)
			adjust_rating_byanchor (Anchor_use, Anchor, General_average, N_players, Flagged, Prefed, Ratingof);

	} //end while 

	*pWhite_advantage = white_adv;
	*pDraw_date = draw_rate;


	free(expected);
	return N_enc;
}

#if 0
static double
overallerrorE_fdrawrate (int N_enc, const struct ENC *enc, double *ratingof, double beta, double wadv, double dr0)
{
	int e, w, b;
	double dp, dp2, f, draws_expected;

	dp2 = 0;
	for (e = 0; e < N_enc; e++) {
		w = enc[e].wh;
		b = enc[e].bl;
		f = xpect (ratingof[w] + wadv, ratingof[b], beta);
		draws_expected = enc[e].played * draw_rate_fperf (f, dr0);
		dp = draws_expected - enc[e].D; 
		dp2 += dp * dp;
	}

	return dp2;
}
#else
static double
overallerrorE_fdrawrate (int N_enc, const struct ENC *enc, double *ratingof, double beta, double wadv, double dr0)
{
	int e, w, b;
	double dp2, f;
	double dexp;

	dp2 = 0;
	for (e = 0; e < N_enc; e++) {
		w = enc[e].wh;
		b = enc[e].bl;
		f = xpect (ratingof[w] + wadv, ratingof[b], beta);

		dexp = draw_rate_fperf (f, dr0);

		dp2 +=
				enc[e].D                   * (1-dexp) * (1-dexp) +
				(enc[e].played - enc[e].D) *    dexp  *    dexp  ;
		;
	}

	return dp2;
}
#endif

static double
adjust_drawrate (double start_wadv, double *ratingof, int N_enc, const struct ENC *enc, double beta)
{
	double delta, wa, ei, ej, ek, dr;

	delta = 0.5;
	wa = start_wadv;

	dr = 0.5;

	do {	

		ei = overallerrorE_fdrawrate (N_enc, enc, ratingof, beta, wa, dr - delta);
		ej = overallerrorE_fdrawrate (N_enc, enc, ratingof, beta, wa, dr + 0    );     
		ek = overallerrorE_fdrawrate (N_enc, enc, ratingof, beta, wa, dr + delta);

		if (ei >= ej && ej <= ek) {
			delta = delta / 2;
		} else
		if (ej >= ei && ei <= ek) {
			dr -= delta;
		} else
		if (ei >= ek && ek <= ej) {
			dr += delta;
		}

	} while (
		delta > 0.0001 
	);
	
	return dr;
}


