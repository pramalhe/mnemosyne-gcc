/**
 * \file rwset.h
 *
 * \brief Implements generic read/write set functions.
 *
 */


#ifndef _RWSET_H
#define _RWSET_H

/*
 * Check if stripe has been read previously.
 */
static inline 
r_entry_t *
mtm_has_read(mtm_tx_t *tx, mode_data_t *modedata, volatile mtm_word_t *lock)
{
	r_entry_t   *r;
	int         i;

	PRINT_DEBUG("==> mtm_has_read(%p[%lu-%lu],%p)\n", tx, 
	            (unsigned long)modedata->start, (unsigned long)modedata->end, lock);

	/* Check status */
	assert(tx->status == TX_ACTIVE);

	/* Look for read */
	r = modedata->r_set.entries;
	for (i = modedata->r_set.nb_entries; i > 0; i--, r++) {
		if (r->lock == lock) {
			/* Return first match*/
			return r;
		}
	}
	return NULL;
}


/*
 * Validate read set (check if all read addresses are still valid now).
 */
static inline 
int 
mtm_validate(mtm_tx_t *tx, mode_data_t *modedata)
{
	r_entry_t  *r;
	int        i;
	mtm_word_t l;

	PRINT_DEBUG("==> mtm_validate(%p[%lu-%lu])\n", tx, 
	            (unsigned long)modedata->start, (unsigned long)modedata->end);

	/* Check status */
	assert(tx->status == TX_ACTIVE);

	/* Validate reads */
	r = modedata->r_set.entries;
	for (i = modedata->r_set.nb_entries; i > 0; i--, r++) {
		/* Read lock */
		l = ATOMIC_LOAD(r->lock);
		/* Unlocked and still the same version? */
		if (LOCK_GET_OWNED(l)) {
			/* Do we own the lock? */
#if DESIGN == WRITE_THROUGH
			if ((mtm_tx_t *)LOCK_GET_ADDR(l) != tx)
#else /* DESIGN != WRITE_THROUGH */
			w_entry_t *w = (w_entry_t *)LOCK_GET_ADDR(l);
			/* Simply check if address falls inside our write set (avoids non-faulting load) */
			if (!(modedata->w_set.entries <= w && 
			    w < modedata->w_set.entries + modedata->w_set.nb_entries))
#endif /* DESIGN != WRITE_THROUGH */
			{
				/* Locked by another transaction: cannot validate */
				return 0;
			}
			/* We own the lock: OK */
		} else {
			if (LOCK_GET_TIMESTAMP(l) != r->version) {
				/* Other version: cannot validate */
				return 0;
			}
			/* Same version: OK */
		}
	}
	return 1;
}


/*
 * (Re)allocate read set entries.
 */
static inline 
void 
mtm_allocate_rs_entries(mtm_tx_t *tx, mode_data_t *data, int extend)
{
	if (extend) {
		/* Extend read set */
		data->r_set.size *= 2;
		PRINT_DEBUG2("==> reallocate read set (%p[%lu-%lu],%d)\n", tx, 
		             (unsigned long)data->start, 
		             (unsigned long)data->end, 
		             data->r_set.size);
		if ((data->r_set.entries = 
		     (r_entry_t *)realloc(data->r_set.entries, 
		                          data->r_set.size * sizeof(r_entry_t))) == NULL) 
		{
			perror("realloc");
			exit(1);
		}
	} else {
		/* Allocate read set */
		if ((data->r_set.entries = 
		     (r_entry_t *)malloc(data->r_set.size * sizeof(r_entry_t))) == NULL) 
		{
			perror("malloc");
			exit(1);
		}
	}
}


/*
 * (Re)allocate write set entries.
 */
static inline 
void 
mtm_allocate_ws_entries(mtm_tx_t *tx, mode_data_t *data, int extend)
{
#if defined(READ_LOCKED_DATA) || defined(CONFLICT_TRACKING)
	int i;
	int first = (extend ? data->w_set.size : 0);
#endif /* defined(READ_LOCKED_DATA) || defined(CONFLICT_TRACKING) */

	if (extend) {
		/* Extend write set */
		data->w_set.size *= 2;
		PRINT_DEBUG("==> reallocate write set (%p[%lu-%lu],%d)\n", tx, 
		            (unsigned long)data->start, (unsigned long)data->end, data->w_set.size);
		if ((data->w_set.entries = 
		     (w_entry_t *)realloc(data->w_set.entries, 
		                          data->w_set.size * sizeof(w_entry_t))) == NULL) 
		{
			perror("realloc");
			exit(1);
		}
	} else {
		/* Allocate write set */
#if CM == CM_PRIORITY
		if (posix_memalign((void **)&data->w_set.entries, 
		                   ALIGNMENT, 
		                   data->w_set.size * sizeof(w_entry_t)) != 0) 
		{
			fprintf(stderr, "Error: cannot allocate aligned memory\n");
			exit(1);
		}
#else /* CM != CM_PRIORITY */
		if ((tx->w_set.entries = 
		     (w_entry_t *)malloc(tx->w_set.size * sizeof(w_entry_t))) == NULL)
		{
			perror("malloc");
			exit(1);
		}
#endif /* CM != CM_PRIORITY */
	}

#if defined(READ_LOCKED_DATA) || defined(CONFLICT_TRACKING)
	/* Initialize fields */
	for (i = first; i < data->w_set.size; i++) {
		data->w_set.entries[i].tx = tx;
	}	
#endif /* defined(READ_LOCKED_DATA) || defined(CONFLICT_TRACKING) */
}

#endif
