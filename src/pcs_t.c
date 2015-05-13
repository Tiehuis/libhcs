/*
 * @file pcs_t.c
 *
 * Implementation of the Paillier Cryptosystem (pcs_t).
 *
 * This scheme is a threshold variant of the Paillier system. It loosely
 * follows the scheme presented in the paper by damgard-jurik, but with a
 * chosen base of 2, rather than the variable s+1. This scheme was written
 * first for simplicity.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gmp.h>
#include "com/util.h"
#include "pcs_t.h"

/* This is simply L(x) when s = 1 */
static void dlog_s(mpz_t n, mpz_t rop, mpz_t op)
{
    mpz_sub_ui(rop, op, 1);
    mpz_divexact(rop, rop, n);
    mpz_mod(rop, rop, n);
}

pcs_t_public_key* pcs_t_init_public_key(void)
{
    pcs_t_public_key *pk = malloc(sizeof(pcs_t_public_key));
    if (!pk) return NULL;

    mpz_inits(pk->n, pk->n2, pk->g, pk->delta, NULL);
    return pk;
}

pcs_t_private_key* pcs_t_init_private_key(void)
{
    pcs_t_private_key *vk = malloc(sizeof(pcs_t_private_key));
    if (!vk) return NULL;

    vk->w = vk->l = 0;
    mpz_inits(vk->v, vk->nm, vk->n,
              vk->n2, vk->d, NULL);
    return vk;
}

/* Look into methods of using multiparty computation to generate these keys
 * and the data so we don't have to have a trusted party for generation. */
int pcs_t_generate_key_pair(pcs_t_public_key *pk, pcs_t_private_key *vk,
        hcs_rand *hr, const unsigned long bits, const unsigned long w,
        const unsigned long l)
{
    /* The paper does describe some bounds on w, l */
    //assert(l / 2 <= w && w <= l);

    vk->vi = malloc(sizeof(mpz_t) * l);
    if (vk->vi == NULL) return 0;

    mpz_t t1, t2, t3, t4;
    mpz_init(t1);
    mpz_init(t2);
    mpz_init(t3);
    mpz_init(t4);

    do {
        mpz_random_safe_prime(t1, t2, hr->rstate, 1 + (bits-1)/2);
        mpz_random_safe_prime(t3, t4, hr->rstate, 1 + (bits-1)/2);
    } while (mpz_cmp(t1, t3) == 0);

    mpz_mul(pk->n, t1, t3);
    mpz_set(vk->n, pk->n);
    mpz_pow_ui(pk->n2, pk->n, 2);
    mpz_set(vk->n2, pk->n2);
    mpz_add_ui(pk->g, pk->n, 1);
    mpz_mul(t3, t2, t4);
    mpz_mul(vk->nm, vk->n, t3);
    mpz_set_ui(t1, 1);
    mpz_set_ui(t2, 0);
    mpz_2crt(vk->d, t1, vk->n, t2, t3);
    mpz_fac_ui(pk->delta, l);

    vk->l = l;
    vk->w = w;
    pk->l = l;
    pk->w = w;

    for (unsigned long i = 0; i < l; ++i)
        mpz_init(vk->vi[i]);

    mpz_clear(t1);
    mpz_clear(t2);
    mpz_clear(t3);
    mpz_clear(t4);

    return 1;
}

void pcs_t_encrypt_r(pcs_t_public_key *pk, mpz_t r, mpz_t rop, mpz_t plain1)
{
    mpz_t t1;
    mpz_init(t1);

    mpz_powm(rop, r, pk->n, pk->n2);
    mpz_powm(t1, pk->g, plain1, pk->n2);
    mpz_mul(rop, rop, t1);
    mpz_mod(rop, rop, pk->n2);

    mpz_clear(t1);
}

void pcs_t_encrypt(pcs_t_public_key *pk, hcs_rand *hr, mpz_t rop, mpz_t plain1)
{
    mpz_t t1;
    mpz_init(t1);

    mpz_random_in_mult_group(t1, hr->rstate, pk->n);
    mpz_powm(rop, t1, pk->n, pk->n2);
    mpz_powm(t1, pk->g, plain1, pk->n2);
    mpz_mul(rop, rop, t1);
    mpz_mod(rop, rop, pk->n2);

    mpz_clear(t1);
}

void pcs_t_reencrypt(pcs_t_public_key *pk, hcs_rand *hr, mpz_t rop, mpz_t op)
{
    mpz_t t1;
    mpz_init(t1);

    mpz_random_in_mult_group(t1, hr->rstate, pk->n);
    mpz_powm(t1, t1, pk->n, pk->n2);
    mpz_mul(rop, op, t1);
    mpz_mod(rop, rop, pk->n2);

    mpz_clear(t1);
}

void pcs_t_ep_add(pcs_t_public_key *pk, mpz_t rop, mpz_t cipher1, mpz_t plain1)
{
    mpz_t t1;
    mpz_init(t1);

    mpz_set(t1, cipher1);
    mpz_powm(rop, pk->g, plain1, pk->n2);
    mpz_mul(rop, rop, t1);
    mpz_mod(rop, rop, pk->n2);

    mpz_clear(t1);
}

void pcs_t_ee_add(pcs_t_public_key *pk, mpz_t rop, mpz_t cipher1, mpz_t cipher2)
{
    mpz_mul(rop, cipher1, cipher2);
    mpz_mod(rop, rop, pk->n2);
}

void pcs_t_ep_mul(pcs_t_public_key *pk, mpz_t rop, mpz_t cipher1, mpz_t plain1)
{
    mpz_powm(rop, cipher1, plain1, pk->n2);
}

pcs_t_poly* pcs_t_init_polynomial(pcs_t_private_key *vk, hcs_rand *hr)
{
    pcs_t_poly *px;

    if ((px = malloc(sizeof(pcs_t_poly))) == NULL)
        goto failure;
    if ((px->coeff = malloc(sizeof(mpz_t) * vk->w)) == NULL)
        goto failure;

    px->n = vk->w;
    mpz_init_set(px->coeff[0], vk->d);
    for (unsigned long i = 1; i < px->n; ++i) {
        mpz_init(px->coeff[i]);
        mpz_urandomm(px->coeff[i], hr->rstate, vk->nm);
    }

    return px;

failure:
    if (px->coeff) free(px->coeff);
    if (px) free(px);
    return NULL;
}

void pcs_t_compute_polynomial(pcs_t_private_key *vk, pcs_t_poly *px, mpz_t rop,
                              const unsigned long x)
{
    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);

    mpz_set(rop, px->coeff[0]);
    for (unsigned long i = 1; i < px->n; ++i) {
        mpz_ui_pow_ui(t1, x + 1, i);        // Correct for server 0-indexing
        mpz_mul(t1, t1, px->coeff[i]);
        mpz_add(rop, rop, t1);
        mpz_mod(rop, rop, vk->nm);
    }

    mpz_clear(t1);
    mpz_clear(t2);
}

void pcs_t_free_polynomial(pcs_t_poly *px)
{
    for (unsigned long i = 0; i < px->n; ++i)
        mpz_clear(px->coeff[i]);
    free(px->coeff);
    free(px);
}


pcs_t_auth_server* pcs_t_init_auth_server(void)
{
    pcs_t_auth_server *au = malloc(sizeof(pcs_t_auth_server));
    if (!au) return NULL;

    mpz_init(au->si);
    return au;
}

void pcs_t_set_auth_server(pcs_t_auth_server *au, mpz_t si, unsigned long i)
{
    mpz_set(au->si, si);
    au->i = i + 1; // Input is assumed to be 0-indexed (from array)
}

/* Compute a servers share and set rop to the result. rop should usually
 * be part of an array so we can call pcs_t_share_combine with ease. */
void pcs_t_share_decrypt(pcs_t_public_key *pk, pcs_t_auth_server *au,
                         mpz_t rop, mpz_t cipher1)
{
    mpz_t t1;
    mpz_init(t1);

    mpz_mul(t1, au->si, pk->delta);
    mpz_mul_ui(t1, t1, 2);
    mpz_powm(rop, cipher1, t1, pk->n2);

    mpz_clear(t1);
}

/* c is expected to be of length vk->l, the number of servers. If the share
 * is not present, then it is expected to be equal to the value zero. */
int pcs_t_share_combine(pcs_t_public_key *pk, mpz_t rop, mpz_t *c)
{
    mpz_t t1, t2, t3;
    mpz_init(t1);
    mpz_init(t2);
    mpz_init(t3);

    mpz_set_ui(rop, 1);
    for (unsigned long i = 0; i < pk->l; ++i) {

        /* Skip zero shares */
        if (mpz_cmp_ui(c[i], 0) == 0)
            continue;

        /* Compute lagrange coefficients */
        mpz_set(t1, pk->delta);
        for (unsigned long j = 0; j < pk->l; ++j) {
            if ((j == i) || mpz_cmp_ui(c[j], 0) == 0)
                continue; /* i' in S\i and non-zero */

            long v = (long)j - (long)i;
            mpz_tdiv_q_ui(t1, t1, (v < 0 ? v*-1 : v));
            if (v < 0) mpz_neg(t1, t1);
            mpz_mul_ui(t1, t1, j + 1);
        }

        mpz_abs(t2, t1);
        mpz_mul_ui(t2, t2, 2);
        mpz_powm(t2, c[i], t2, pk->n2);

        if (mpz_sgn(t1) < 0 && !mpz_invert(t2, t2, pk->n2))
	        return 0;

        mpz_mul(rop, rop, t2);
        mpz_mod(rop, rop, pk->n2);
    }

    /* rop = c' */
    dlog_s(pk->n, rop, rop);
    mpz_pow_ui(t1, pk->delta, 2);
    mpz_mul_ui(t1, t1, 4);

    if (!mpz_invert(t1, t1, pk->n))
		return 0;

    mpz_mul(rop, rop, t1);
    mpz_mod(rop, rop, pk->n);

    mpz_clear(t1);
    mpz_clear(t2);
    mpz_clear(t3);
    return 1;
}

void pcs_t_free_auth_server(pcs_t_auth_server *au)
{
    mpz_clear(au->si);
    free(au);
}

void pcs_t_clear_public_key(pcs_t_public_key *pk)
{
    mpz_zeros(pk->g, pk->n, pk->n2, pk->delta, NULL);
}

void pcs_t_clear_private_key(pcs_t_private_key *vk)
{
    mpz_zeros(vk->v, vk->nm, vk->n,
              vk->n2, vk->d, NULL);

    if (vk->vi) {
        for (unsigned long i = 0; i < vk->l; ++i)
            mpz_clear(vk->vi[i]);
        free (vk->vi);
    }
}

void pcs_t_free_public_key(pcs_t_public_key *pk)
{
    mpz_clears(pk->g, pk->n, pk->n2, pk->delta, NULL);
    free(pk);
}

void pcs_t_free_private_key(pcs_t_private_key *vk)
{
    mpz_clears(vk->v, vk->nm, vk->n,
               vk->n2, vk->d, NULL);

    if (vk->vi) {
        for (unsigned long i = 0; i < vk->l; ++i)
            mpz_clear(vk->vi[i]);
        free (vk->vi);
    }

    free(vk);
}

#ifdef MAIN

#define AS_REQ   4
#define AS_COUNT 7
#define V_COUNT 50

#define loop_i(high) for (int i = 0; i < (high); ++i)

int main(void)
{
    /* Shared variables used for sending values between users */
    mpz_t svar1, svar2;
    mpz_init(svar1);
    mpz_init(svar2);

    /* Shared for simplicity: Originates from VM */
    hcs_rand *hr = hcs_rand_init();
    pcs_t_public_key *pk = pcs_t_init_public_key();

    /* VM: Initial key setup */
    pcs_t_private_key *vk = pcs_t_init_private_key();
    pcs_t_generate_key_pair(pk, vk, hr, 128, AS_REQ, AS_COUNT);

    /* Seperate AS: AS setup */
    pcs_t_auth_server *ai[AS_COUNT];
    loop_i(AS_COUNT) {
        ai[i] = pcs_t_init_auth_server();
    }

    /* VM: Initialise polynomial */
    hcs_poly *coeff = pcs_t_init_polynomial(vk);

    /* Seperate AS: Each AS sends a request for a polynomial value
     * This is not usually in order. */
    loop_i(AS_COUNT) {

        /* VM: Compute polynomial, storing in shared variable */
        pcs_t_compute_polynomial(vk, coeff, svar1, i);

        /* AS: Grab return id and value and set */
        pcs_t_set_auth_server(au[i], svar1, i);
    }

    /* VM: Send a copy of the public key to the Board, discard private key. */
    pcs_t_free_private_key(vk);

    /* BOARD: Post public key on board values */

    /* Seperate VOTERS: Each voter initialises their state, and obtains the
     * public key from the board. */
    mpz_t voter[V_COUNT];
    loop_i(V_COUNT) {
        mpz_init(voter[i]);
    }

    /* Seperate VOTERS: Choose their vote and send the value, along with the
     * proof of their value to the server. */
    loop_i(V_COUNT) {
        mpz_set_ui(svar2, 2);
        mpz_urandomm(svar1, hr->rstate, svar2);

        /* Each voter encrypts their value with the public key this cannot
         * be reversed without AS_REQ servers. */
        voter[i] = pcs_t_encrypt(pk, hr, svar1, svar1);

        /* BOARD: value is sent to board, along with a zero-knowledge proof
         * for the value of the number. The proof can be checked here,
         * although, it is more likely to be checked during tally, and/or
         * during the voting phase. Invalid proof/vote pairs will still be
         * posted, so people can check to see if there are excessive
         * invalid votes posted. */

        // if (!pcs_t_zproof(svar1, proof)) return INVALID;

        /* BOARD: If the value is valid, we add that to the current vote
         * table. In this case, the votes are just stored in voter. */
    }

    /* BOARD: Once the voting stage is done, we prevent any new votes from
     * being added. */

    // voting.cease()

    /* Anyone can tally the votes, as all voters have the private key */
    mpz_set_ui(svar1, 0);
    pcs_t_encrypt(pk, hr, svar1, svar1);
    loop_i(V_COUNT) {
        /* Seperate Voter: Sum the votes and post the result. Every voter
         * can do this for self-verification that the tally is indeed
         * accurate. */
        pcs_t_ee_add(pk, svar1, svar1, voter[i]);
    }

    mpz_t board_shares[AS_COUNT];
    loop_i(AS_COUNT) {
        mpz_init(board_shares[i]);
    }

    /* svar1 stores vote count, now the pcs_t_auth_servers each compute
     * their shares and post their results on the board. */
    loop_i(AS_COUNT) {
        pcs_t_share_decrypt(pk, au[i], board_shares[i], svar1);
    }

    /* All the shares are contained on the board. Anyone can now sum the
     * results and write the combined values to the board. Multiple share
     * combinations should be taken to ensure that the value we get is
     * indeed correct. */
    loop_i(V_COUNT) {
        /* Confirm we get the same output as others have posted. Confirm that
         * different orderings indeed provide the same results. If not, test
         * more combinations to determine the share that is invalid. */
        pcs_t_share_combine(pk, svar2, board_shares);
    }

    /* We have succesfully completed a vote. Cleanup. */

}

#endif
