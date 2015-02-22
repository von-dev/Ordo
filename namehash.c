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
#include <string.h>

#include "namehash.h"
#include "pgnget.h"

//#define PEAXPOD 8  //FIXME true values
//#define PODBITS 12 //FIXME true values
#define PEAXPOD 1
#define PODBITS 1
#define PODMASK ((1<<PODBITS)-1)
#define PODMAX   (1<<PODBITS)
#define PEA_REM_MAX (256*256)

struct NAMEPEA {
	player_t pidx; // player index
	uint32_t hash; // name hash
};

struct NAMEPOD {
	struct NAMEPEA pea[PEAXPOD];
	int n;
};


// ----------------- PRIVATE DATA---------------
static struct NAMEPOD Namehashtab[PODMAX];
//----------------------------------------------


#if 0
static void
hashstat(void)
{
	int i, level;
	int hist[9] = {0,0,0,0,0,0,0,0,0};

	for (i = 0; i < PODMAX; i++) {
		level = Namehashtab[i].n;
		hist[level]++;
	}
	for (i = 0; i < 9; i++) {
		printf ("level[%d]=%d\n",i,hist[i]);
	}
}
#endif

static bool_t name_ispresent_hashtable (struct DATA *d, const char *s, uint32_t hash, /*out*/ player_t *out_index);
static bool_t name_register_hashtable (uint32_t hash, player_t i);
static bool_t name_ispresent_tree (struct DATA *d, const char *s, uint32_t hash, /*out*/ player_t *out_index);
static bool_t name_register_tree (uint32_t hash, player_t i);

#if 0
static bool_t name_ispresent_tail (struct DATA *d, const char *s, uint32_t hash, /*out*/ player_t *out_index);
static bool_t name_register_tail (uint32_t hash, player_t i);
#endif

bool_t
name_ispresent (struct DATA *d, const char *s, uint32_t hash, /*out*/ player_t *out_index)
{
	if (name_ispresent_hashtable(d,s,hash,out_index)) {
		return TRUE;
	} else if (name_ispresent_tree(d,s,hash,out_index)) {
		return TRUE;
	}
	return FALSE;
}


bool_t
name_register (uint32_t hash, player_t i)
{
	if (name_register_hashtable (hash, i)) {
		return TRUE;
	} else if (name_register_tree (hash, i)) {
		return TRUE;
	}else {
		return FALSE;
	}
}

//**************************************************************************

static bool_t
name_ispresent_hashtable (struct DATA *d, const char *s, uint32_t hash, /*out*/ player_t *out_index)
{
	struct NAMEPOD *ppod = &Namehashtab[hash & PODMASK];
	struct NAMEPEA *ppea;
	int 			n;
	bool_t 			found= FALSE;
	int i;

	ppea = ppod->pea;
	n = ppod->n;
	for (i = 0; i < n; i++) {
		if (ppea[i].hash == hash) {
			const char *name_str = database_getname(d, ppea[i].pidx);
			assert(name_str);
			if (!strcmp(s, name_str)) {
				found = TRUE;
				*out_index = ppea[i].pidx;
				break;
			}
		}
	}
	return found;
}

bool_t
name_register_hashtable (uint32_t hash, player_t i)
{
	struct NAMEPOD *ppod = &Namehashtab[hash & PODMASK];
	struct NAMEPEA *ppea;
	int 			n;

	ppea = ppod->pea;
	n = ppod->n;	

	if (n < PEAXPOD) {
		ppea[n].pidx = i;
		ppea[n].hash = hash;
		ppod->n++;
		return TRUE;
	}
	else {
		return FALSE;
	}
}

//**************************************************************************
struct NODETREE {
	struct NODETREE *hi;
	struct NODETREE *lo;
	struct NAMEPEA p;
};

// ----------------- PRIVATE DATA---------------
static struct NODETREE Tremains[PEA_REM_MAX];
static player_t Tremains_n;
static struct NODETREE *Troot = NULL;
//----------------------------------------------

static void nodetree_connect (struct NODETREE *root, struct NODETREE *pnew);
static int	nodetree_cmp (struct NODETREE *a, struct NODETREE *b);
static bool_t nodetree_is_hit (const struct DATA *d, const char *s, uint32_t hash, const struct NODETREE *pnode);

static bool_t
name_register_tree (uint32_t hash, player_t i)
{
	player_t n = Tremains_n;
	if (n < PEA_REM_MAX) {
		if (n == 0) Troot = Tremains; // init
		Tremains[n].hi = NULL;
		Tremains[n].lo = NULL;		
		Tremains[n].p.pidx = i;
		Tremains[n].p.hash = hash;
		if (n > 0)
			nodetree_connect (Tremains, &Tremains[n]);
		Tremains_n++;
		return TRUE;
	}
	else {
		return FALSE;
	}
}


static bool_t
name_ispresent_tree (struct DATA *d, const char *s, uint32_t hash, /*out*/ player_t *out_index)
{
	struct NODETREE *pnode = Troot;
	for (;;) {
		if (pnode == NULL) return FALSE;
		if (nodetree_is_hit (d, s, hash, pnode)) {
			*out_index = pnode->p.pidx;
			return TRUE;
		} else if (hash < pnode->p.hash) {
			pnode = pnode->lo;
		} else {
			pnode = pnode->hi;			
		}	
	}
}


static int
nodetree_cmp (struct NODETREE *a, struct NODETREE *b)
{
	if (a->p.hash < b->p.hash) return -1;
	if (a->p.hash > b->p.hash) return +1;
	if (a->p.pidx < b->p.pidx) return -1;
	if (a->p.pidx > b->p.pidx) return +1;
	return 0;	
}

static void
nodetree_connect (struct NODETREE *root, struct NODETREE *pnew)
{
	struct NODETREE *r = root;
	bool_t done = FALSE;
	while (!done) {
		if (0 > nodetree_cmp(pnew,r)) {
			if (r->lo == NULL) {
				r->lo = pnew;
				done = TRUE;
			} else {
				r = r->lo;
			}	
		} else {
			if (r->hi == NULL) {
				r->hi = pnew;
				done = TRUE;
			} else {
				r = r->hi;
			}	
		}
	}
	return;
}

static bool_t
nodetree_is_hit (const struct DATA *d, const char *s, uint32_t hash, const struct NODETREE *pnode)
{
	return hash == pnode->p.hash && !strcmp(s, database_getname(d, pnode->p.pidx));
}

//**************************************************************************

#if 0
// ----------------- PRIVATE DATA---------------
static struct NAMEPEA Nameremains[PEA_REM_MAX];
static player_t Nameremains_n;
//----------------------------------------------

static bool_t
name_ispresent_tail (struct DATA *d, const char *s, uint32_t hash, /*out*/ player_t *out_index)
{
	struct NAMEPEA *ppea;
	player_t		n;
	bool_t 			found= FALSE;
	int i;

	ppea = Nameremains;
	n = Nameremains_n;
	for (i = 0; i < n; i++) {
		if (ppea[i].hash == hash && !strcmp(s, database_getname(d, ppea[i].pidx))) {
			found = TRUE;
			*out_index = ppea[i].pidx;
			break;
		}
	}

	return found;
}

static bool_t
name_register_tail (uint32_t hash, player_t i)
{
	if (Nameremains_n < PEA_REM_MAX) {
		Nameremains[Nameremains_n].pidx = i;
		Nameremains[Nameremains_n].hash = hash;
		Nameremains_n++;
		return TRUE;
	}
	else {
		return FALSE;
	}
}
#endif

/************************************************************************/

/*http://www.cse.yorku.ca/~oz/hash.html*/

uint32_t
namehash(const char *str)
{
	uint32_t hash = 5381;
	char chr;
	unsigned int c;
	while ('\0' != *str) {
		chr = *str++;
		c = (unsigned int) ((unsigned char)(chr));
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}
	return hash;
}

/************************************************************************/


