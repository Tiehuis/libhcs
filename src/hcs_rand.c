/**
 * @file hcs_rand.c
 *
 * Provides easy to use random state functions, that are required
 * for all probablistic schemes.
 *
 * This is effectively a wrapper around a gmp_randstate_t type. However, we
 * provide safe seeding of the generator, instead of the user having to.
 * This is provided as a new struct to more closely resemble the usage of the
 * other types within this library.
 */

#include <gmp.h>
#include "hcs_rand.h"
#include "com/util.h"

/* Currently one can set the seed. This is used only for testing and will
   be altered at a latter time to take no arguments. */
hcs_rand* hcs_rand_init(const unsigned long v)
{
    hcs_rand *hr = malloc(sizeof(hcs_rand));
    if (hr == NULL) return NULL;

    mpz_t t1;
    mpz_init_set_ui(t1, v);

    gmp_randinit_default(hr->rstate);
#if 0 // Comment out to zero seed for testing
    mpz_seed(t1, HCS_RAND_SEED_BITS);
#endif
    gmp_randseed(hr->rstate, t1);

    mpz_clear(t1);
    return hr;
}

int hcs_rand_reseed(hcs_rand *hr)
{
    // Currently assume we get values correctly. We should check that we
    // read correctly and alter mpz_seed to return a status code.
    mpz_t t1;
    mpz_init(t1);

    if (mpz_seed(t1, HCS_RAND_SEED_BITS) != HCS_OK) {
        mpz_clear(t1);
        return 0;
    }

    gmp_randseed(hr->rstate, t1);
    mpz_clear(t1);
    return 1;
}

void hcs_rand_free(hcs_rand *hr)
{
    gmp_randclear(hr->rstate);
    free(hr);
}