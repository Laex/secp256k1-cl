// Copyright (c) 2013 Pieter Wuille
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <assert.h>
#include "num_impl.h"
#include "field_impl.h"
#include "group_impl.h"
#include "ecmult_impl.h"
#include "ecdsa_impl.h"
#include "CL/cl.h"
#include "sigs.h"
#include <windows.h>

void secp256k1_start(void) {
    secp256k1_fe_start();
    secp256k1_ge_start();
    secp256k1_ecmult_start();
}

void secp256k1_stop(void) {
    secp256k1_ecmult_stop();
    secp256k1_ge_stop();
    secp256k1_fe_stop();
}

int secp256k1_ecdsa_verify(const unsigned char *msg, int msglen, const unsigned char *sig, int siglen, const unsigned char *pubkey, int pubkeylen) {
    int ret = -3;
    secp256k1_num_t m; 
    secp256k1_num_init(&m);
    secp256k1_ecdsa_sig_t s;
    secp256k1_ecdsa_sig_init(&s);
    secp256k1_ge_t q;
    secp256k1_num_set_bin(&m, msg, msglen);

    if (!secp256k1_ecdsa_pubkey_parse(&q, pubkey, pubkeylen)) {
        ret = -1;
        goto end;
    }
    if (!secp256k1_ecdsa_sig_parse(&s, sig, siglen)) {
        ret = -2;
        goto end;
    }
    if (!secp256k1_ecdsa_sig_verify(&s, &q, &m)) {
        ret = 0;
        goto end;
    }
    ret = 1;
end:
    secp256k1_ecdsa_sig_free(&s);
    secp256k1_num_free(&m);
    return ret;
}

int secp256k1_ecdsa_sign(const unsigned char *message, int messagelen, unsigned char *signature, int *signaturelen, const unsigned char *seckey, const unsigned char *nonce) {
    secp256k1_num_t sec, non, msg;
    secp256k1_num_init(&sec);
    secp256k1_num_init(&non);
    secp256k1_num_init(&msg);
    secp256k1_num_set_bin(&sec, seckey, 32);
    secp256k1_num_set_bin(&non, nonce, 32);
    secp256k1_num_set_bin(&msg, message, messagelen);
    int ret = !secp256k1_num_is_zero(&non) &&
              (secp256k1_num_cmp(&non, &secp256k1_ge_consts->order) < 0);
    secp256k1_ecdsa_sig_t sig;
    secp256k1_ecdsa_sig_init(&sig);
    if (ret) {
        ret = secp256k1_ecdsa_sig_sign(&sig, &sec, &msg, &non, NULL);
    }
    if (ret) {
        secp256k1_ecdsa_sig_serialize(signature, signaturelen, &sig);
    }
    secp256k1_ecdsa_sig_free(&sig);
    secp256k1_num_free(&msg);
    secp256k1_num_free(&non);
    secp256k1_num_free(&sec);
    return ret;
}

int secp256k1_ecdsa_sign_compact(const unsigned char *message, int messagelen, unsigned char *sig64, const unsigned char *seckey, const unsigned char *nonce, int *recid) {
    secp256k1_num_t sec, non, msg;
    secp256k1_num_init(&sec);
    secp256k1_num_init(&non);
    secp256k1_num_init(&msg);
    secp256k1_num_set_bin(&sec, seckey, 32);
    secp256k1_num_set_bin(&non, nonce, 32);
    secp256k1_num_set_bin(&msg, message, messagelen);
    int ret = !secp256k1_num_is_zero(&non) &&
              (secp256k1_num_cmp(&non, &secp256k1_ge_consts->order) < 0);
    secp256k1_ecdsa_sig_t sig;
    secp256k1_ecdsa_sig_init(&sig);
    if (ret) {
        ret = secp256k1_ecdsa_sig_sign(&sig, &sec, &msg, &non, recid);
    }
    if (ret) {
        secp256k1_num_get_bin(sig64, 32, &sig.r);
        secp256k1_num_get_bin(sig64 + 32, 32, &sig.s);
    }
    secp256k1_ecdsa_sig_free(&sig);
    secp256k1_num_free(&msg);
    secp256k1_num_free(&non);
    secp256k1_num_free(&sec);
    return ret;
}

int secp256k1_ecdsa_recover_compact(const unsigned char *msg, int msglen, const unsigned char *sig64, unsigned char *pubkey, int *pubkeylen, int compressed, int recid) {
    int ret = 0;
    secp256k1_num_t m; 
    secp256k1_num_init(&m);
    secp256k1_ecdsa_sig_t sig;
    secp256k1_ecdsa_sig_init(&sig);
    secp256k1_num_set_bin(&sig.r, sig64, 32);
    secp256k1_num_set_bin(&sig.s, sig64 + 32, 32);
    secp256k1_num_set_bin(&m, msg, msglen);

    secp256k1_ge_t q;
    if (secp256k1_ecdsa_sig_recover(&sig, &q, &m, recid)) {
        secp256k1_ecdsa_pubkey_serialize(&q, pubkey, pubkeylen, compressed);
        ret = 1;
    }
    secp256k1_ecdsa_sig_free(&sig);
    secp256k1_num_free(&m);
    return ret;
}

int secp256k1_ecdsa_seckey_verify(const unsigned char *seckey) {
    secp256k1_num_t sec;
    secp256k1_num_init(&sec);
    secp256k1_num_set_bin(&sec, seckey, 32);
    int ret = !secp256k1_num_is_zero(&sec) &&
              (secp256k1_num_cmp(&sec, &secp256k1_ge_consts->order) < 0);
    secp256k1_num_free(&sec);
    return ret;
}

int secp256k1_ecdsa_pubkey_verify(const unsigned char *pubkey, int pubkeylen) {
    secp256k1_ge_t q;
    return secp256k1_ecdsa_pubkey_parse(&q, pubkey, pubkeylen);
}

int secp256k1_ecdsa_pubkey_create(unsigned char *pubkey, int *pubkeylen, const unsigned char *seckey, int compressed) {
    secp256k1_num_t sec;
    secp256k1_num_init(&sec);
    secp256k1_num_set_bin(&sec, seckey, 32);
    secp256k1_gej_t pj;
    secp256k1_ecmult_gen(&pj, &sec);
    secp256k1_ge_t p;
    secp256k1_ge_set_gej(&p, &pj);
    secp256k1_ecdsa_pubkey_serialize(&p, pubkey, pubkeylen, compressed);
    return 1;
}

int secp256k1_ecdsa_pubkey_decompress(unsigned char *pubkey, int *pubkeylen) {
    secp256k1_ge_t p;
    if (!secp256k1_ecdsa_pubkey_parse(&p, pubkey, *pubkeylen))
        return 0;
    secp256k1_ecdsa_pubkey_serialize(&p, pubkey, pubkeylen, 0);
    return 1;
}

int secp256k1_ecdsa_privkey_tweak_add(unsigned char *seckey, const unsigned char *tweak) {
    int ret = 1;
    secp256k1_num_t term;
    secp256k1_num_init(&term);
    secp256k1_num_set_bin(&term, tweak, 32);
    if (secp256k1_num_cmp(&term, &secp256k1_ge_consts->order) >= 0)
        ret = 0;
    secp256k1_num_t sec;
    secp256k1_num_init(&sec);
    if (ret) {
        secp256k1_num_set_bin(&sec, seckey, 32);
        secp256k1_num_add(&sec, &sec, &term);
        secp256k1_num_mod(&sec, &secp256k1_ge_consts->order);
        if (secp256k1_num_is_zero(&sec))
            ret = 0;
    }
    if (ret)
        secp256k1_num_get_bin(seckey, 32, &sec);
    secp256k1_num_free(&sec);
    secp256k1_num_free(&term);
    return ret;
}

int secp256k1_ecdsa_pubkey_tweak_add(unsigned char *pubkey, int pubkeylen, const unsigned char *tweak) {
    int ret = 1;
    secp256k1_num_t term;
    secp256k1_num_init(&term);
    secp256k1_num_set_bin(&term, tweak, 32);
    if (secp256k1_num_cmp(&term, &secp256k1_ge_consts->order) >= 0)
        ret = 0;
    secp256k1_ge_t p;
    if (ret) {
        if (!secp256k1_ecdsa_pubkey_parse(&p, pubkey, pubkeylen))
            ret = 0;
    }
    if (ret) {
        secp256k1_gej_t pt;
        secp256k1_ecmult_gen(&pt, &term);
        secp256k1_gej_add_ge(&pt, &pt, &p);
        if (secp256k1_gej_is_infinity(&pt))
            ret = 0;
        secp256k1_ge_set_gej(&p, &pt);
        int oldlen = pubkeylen;
        secp256k1_ecdsa_pubkey_serialize(&p, pubkey, &pubkeylen, oldlen <= 33);
        assert(pubkeylen == oldlen);
    }
    secp256k1_num_free(&term);
    return ret;
}

int secp256k1_ecdsa_privkey_tweak_mul(unsigned char *seckey, const unsigned char *tweak) {
    int ret = 1;
    secp256k1_num_t factor;
    secp256k1_num_init(&factor);
    secp256k1_num_set_bin(&factor, tweak, 32);
    if (secp256k1_num_is_zero(&factor))
        ret = 0;
    if (secp256k1_num_cmp(&factor, &secp256k1_ge_consts->order) >= 0)
        ret = 0;
    secp256k1_num_t sec;
    secp256k1_num_init(&sec);
    if (ret) {
        secp256k1_num_set_bin(&sec, seckey, 32);
        secp256k1_num_mod_mul(&sec, &sec, &factor, &secp256k1_ge_consts->order);
    }
    if (ret)
        secp256k1_num_get_bin(seckey, 32, &sec);
    secp256k1_num_free(&sec);
    secp256k1_num_free(&factor);
    return ret;
}

int secp256k1_ecdsa_pubkey_tweak_mul(unsigned char *pubkey, int pubkeylen, const unsigned char *tweak) {
    int ret = 1;
    secp256k1_num_t factor;
    secp256k1_num_init(&factor);
    secp256k1_num_set_bin(&factor, tweak, 32);
    if (secp256k1_num_is_zero(&factor))
        ret = 0;
    if (secp256k1_num_cmp(&factor, &secp256k1_ge_consts->order) >= 0)
        ret = 0;
    secp256k1_ge_t p;
    if (ret) {
        if (!secp256k1_ecdsa_pubkey_parse(&p, pubkey, pubkeylen))
            ret = 0;
    }
    if (ret) {
        secp256k1_num_t zero;
        secp256k1_num_init(&zero);
        secp256k1_num_set_int(&zero, 0);
        secp256k1_gej_t pt;
        secp256k1_gej_set_ge(&pt, &p);
        secp256k1_ecmult(&pt, &pt, &factor, &zero);
        secp256k1_num_free(&zero);
        secp256k1_ge_set_gej(&p, &pt);
        int oldlen = pubkeylen;
        secp256k1_ecdsa_pubkey_serialize(&p, pubkey, &pubkeylen, oldlen <= 33);
        assert(pubkeylen == oldlen);
    }
    secp256k1_num_free(&factor);
    return ret;
}

int secp256k1_ecdsa_privkey_export(const unsigned char *seckey, unsigned char *privkey, int *privkeylen, int compressed) {
    secp256k1_num_t key;
    secp256k1_num_init(&key);
    secp256k1_num_set_bin(&key, seckey, 32);
    int ret = secp256k1_ecdsa_privkey_serialize(privkey, privkeylen, &key, compressed);
    secp256k1_num_free(&key);
    return ret;
}

int secp256k1_ecdsa_privkey_import(unsigned char *seckey, const unsigned char *privkey, int privkeylen) {
    secp256k1_num_t key;
    secp256k1_num_init(&key);
    int ret = secp256k1_ecdsa_privkey_parse(&key, privkey, privkeylen);
    if (ret)
        secp256k1_num_get_bin(seckey, 32, &key);
    secp256k1_num_free(&key);
    return ret;
}

typedef struct {
	secp256k1_gej_t a;
	char wnaf_na[257];
	short bits_na;
	short wnaf_ng_1[129];
	short bits_ng_1;
	short wnaf_ng_128[129];
	short bits_ng_128;
} ecmult_params_device_t;

typedef struct {
	ecmult_params_device_t d;
	secp256k1_num_t r;
} ecmult_params_t;

// static inline int max(int a, int b) { return a > b ? a : b; }

void ec_mult(secp256k1_gej_t *pr, ecmult_params_t *pParams) {
    const secp256k1_ecmult_consts_t *mc = secp256k1_ecmult_consts;
    secp256k1_gej_t pre_a[ECMULT_TABLE_SIZE(WINDOW_A)];
    secp256k1_ecmult_table_precomp_gej(pre_a, &pParams->d.a, WINDOW_A);

    secp256k1_gej_set_infinity(pr);
    secp256k1_gej_t tmpj;
    secp256k1_ge_t tmpa;

    int bits = max(pParams->d.bits_na, pParams->d.bits_ng_1);
    bits = max(bits, pParams->d.bits_ng_128);
    for (int i=bits-1; i>=0; i--) {
        secp256k1_gej_double(pr, pr);
        int n;
        if (i < pParams->d.bits_na && (n = pParams->d.wnaf_na[i])) {
            ECMULT_TABLE_GET_GEJ(&tmpj, pre_a, n, WINDOW_A);
            secp256k1_gej_add(pr, pr, &tmpj);
        }

        if (i < pParams->d.bits_ng_1 && (n = pParams->d.wnaf_ng_1[i])) {
            ECMULT_TABLE_GET_GE(&tmpa, mc->pre_g, n, WINDOW_G);
            secp256k1_gej_add_ge(pr, pr, &tmpa);
        }
        if (i < pParams->d.bits_ng_128 && (n = pParams->d.wnaf_ng_128[i])) {
            ECMULT_TABLE_GET_GE(&tmpa, mc->pre_g_128, n, WINDOW_G);
            secp256k1_gej_add_ge(pr, pr, &tmpa);
        }
    }
}

int *secp256k1_ecdsa_verify_batch(cl_context context, cl_command_queue command_queue, cl_kernel ecmult_table_precomp_gej_kernel, cl_kernel ecmult_kernel,
		int sigsLen, const Sig *sigs) {
	int *prets = malloc(sigsLen * sizeof(int));
    int ret = -3;

    int work_item_size = sizeof(ecmult_params_t);

    const secp256k1_ge_consts_t *c = secp256k1_ge_consts;
    ecmult_params_t *ecmult_params = malloc(sigsLen * sizeof(ecmult_params_t));
    const Sig *pSig = sigs;
    ecmult_params_t *pParams = ecmult_params;

	LARGE_INTEGER q1, q2, q3, q4, q5, ticksPerSecond;
	QueryPerformanceFrequency(&ticksPerSecond);
	QueryPerformanceCounter(&q1);

    for (int i = 0; i < sigsLen; i++) {
		secp256k1_num_t m;
		secp256k1_num_init(&m);
		secp256k1_ecdsa_sig_t s;
		secp256k1_ecdsa_sig_init(&s);
		secp256k1_ge_t q;
		secp256k1_num_set_bin(&m, pSig->hash, pSig->hashLen);

		if (!secp256k1_ecdsa_pubkey_parse(&q, pSig->pubkey, pSig->pubkeyLen)) {
			prets[i] = -1;
			goto end;
		}
		if (!secp256k1_ecdsa_sig_parse(&s, pSig->signature, pSig->signatureLen)) {
			prets[i] = -2;
			goto end;
		}

		assert(m.limbs * sizeof(m.data[0]) <= 32);
		assert(s.r.limbs * sizeof(s.r.data[0]) <= 32);
		assert(s.s.limbs * sizeof(s.s.data[0]) <= 32);

	    secp256k1_num_t sn, u1, u2;
	    secp256k1_num_init(&sn);
	    secp256k1_num_init(&u1);
	    secp256k1_num_init(&u2);
	    secp256k1_num_mod_inverse(&sn, &s.s, &c->order);
	    secp256k1_num_mod_mul(&u1, &sn, &m, &c->order);
	    secp256k1_num_mod_mul(&u2, &sn, &s.r, &c->order);

	    // w = 1/s
	    // u1 = zw, u2 = rw
	    // u1.G + u2.Q -> na = u2, ng = u1
	    secp256k1_gej_t pubkeyj; secp256k1_gej_set_ge(&pubkeyj, &q);

	    memcpy(&pParams->d.a, &pubkeyj, sizeof(secp256k1_gej_t));
	    // memcpy(&pParams->na, &u2, sizeof(secp256k1_num_t));
	    // memcpy(&pParams->ng, &u1, sizeof(secp256k1_num_t));
	    memcpy(&pParams->r, &s.r, sizeof(secp256k1_num_t));

	    int wnaf_na[257];
	    pParams->d.bits_na = secp256k1_ecmult_wnaf(wnaf_na, &u2, WINDOW_A);
	    for (int i = 0; i < 257; i++)
	    	pParams->d.wnaf_na[i] = (char)wnaf_na[i];

	    // Splitted G factors.
	    secp256k1_num_t ng_1, ng_128;
	    secp256k1_num_init(&ng_1);
	    secp256k1_num_init(&ng_128);

	    // split ng into ng_1 and ng_128 (where gn = gn_1 + gn_128*2^128, and gn_1 and gn_128 are ~128 bit)
	    secp256k1_num_split(&ng_1, &ng_128, &u1, 128);

	    // Build wnaf representation for ng_1 and ng_128
	    int wnaf_ng_1[129];   pParams->d.bits_ng_1   = secp256k1_ecmult_wnaf(wnaf_ng_1,   &ng_1,   WINDOW_G);
	    int wnaf_ng_128[129]; pParams->d.bits_ng_128 = secp256k1_ecmult_wnaf(wnaf_ng_128, &ng_128, WINDOW_G);
	    for (int i = 0; i < 129; i++) {
	    	pParams->d.wnaf_ng_1[i] = (short)wnaf_ng_1[i];
	    	pParams->d.wnaf_ng_128[i] = (short)wnaf_ng_128[i];
	    }

	    end:
	    pParams++;
		pSig++;
    }

    cl_int cl_ret;
    const secp256k1_ecmult_consts_t *mc = secp256k1_ecmult_consts;
    cl_mem pmc_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(secp256k1_ecmult_consts_t), mc, &cl_ret); // No need to do it everytime...
	QueryPerformanceCounter(&q2);

    secp256k1_gej_t *pr;
#ifdef USE_CPU
    secp256k1_gej_t *pr0 = malloc(sigsLen * sizeof(secp256k1_gej_t));
    pr = pr0;

    // Perform EC multiplication and sum
    pParams = ecmult_params;
    for (int i = 0; i < sigsLen; i++) {
    	ec_mult(pr, pParams);

    	pParams++;
    	pr++;
    }
#endif

    int tableSize = ECMULT_TABLE_SIZE(WINDOW_A);
    cl_mem a_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(secp256k1_gej_t) * sigsLen, NULL, &cl_ret);
    cl_mem prea_mem_obj = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(secp256k1_gej_t) * tableSize * sigsLen, NULL, &cl_ret);

    secp256k1_gej_t *as = (secp256k1_gej_t *)malloc(sigsLen * sizeof(secp256k1_gej_t));
    for (int i = 0; i < sigsLen; i++)
    	memcpy(&as[i], &ecmult_params[i].d.a, sizeof(secp256k1_gej_t));

    ret = clEnqueueWriteBuffer(command_queue, a_mem_obj, CL_TRUE, 0,
    		sizeof(secp256k1_gej_t) * sigsLen, as, 0, NULL, NULL);
    free(as);

    int w = WINDOW_A;
    cl_ret = clSetKernelArg(ecmult_table_precomp_gej_kernel, 0, sizeof(cl_mem), (void *)&prea_mem_obj);
    cl_ret = clSetKernelArg(ecmult_table_precomp_gej_kernel, 1, sizeof(cl_mem), (void *)&a_mem_obj);
    cl_ret = clSetKernelArg(ecmult_table_precomp_gej_kernel, 2, sizeof(int), (void *)&w);

    size_t global_item_size = sigsLen; // Process the entire lists
    size_t local_item_size = 256; // Divide work items into groups of 64
    cl_ret = clEnqueueNDRangeKernel(command_queue, ecmult_table_precomp_gej_kernel, 1, NULL,
            &global_item_size, &local_item_size, 0, NULL, NULL);

    ret = clReleaseMemObject(a_mem_obj);

    cl_mem pr_mem_obj = clCreateBuffer(context, CL_MEM_READ_WRITE, 32 * sigsLen, NULL, &cl_ret);
    cl_mem pparams_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(ecmult_params_device_t) * sigsLen, NULL, &cl_ret);
    ecmult_params_device_t *pparams_device = malloc(sizeof(ecmult_params_device_t) * sigsLen);
    for (int i = 0; i < sigsLen; i++) {
    	memcpy(&pparams_device[i], &ecmult_params[i].d, sizeof(ecmult_params_device_t));
    }
    ret = clEnqueueWriteBuffer(command_queue, pparams_mem_obj, CL_TRUE, 0,
    		sizeof(ecmult_params_device_t) * sigsLen, pparams_device, 0, NULL, NULL);
    free(pparams_device);

    cl_ret = clSetKernelArg(ecmult_kernel, 0, sizeof(cl_mem), (void *)&pr_mem_obj);
    cl_ret = clSetKernelArg(ecmult_kernel, 1, sizeof(cl_mem), (void *)&prea_mem_obj);
    cl_ret = clSetKernelArg(ecmult_kernel, 2, sizeof(cl_mem), (void *)&pparams_mem_obj);
    cl_ret = clSetKernelArg(ecmult_kernel, 3, sizeof(cl_mem), (void *)&pmc_mem_obj);

    cl_ret = clEnqueueNDRangeKernel(command_queue, ecmult_kernel, 1, NULL,
            &global_item_size, &local_item_size, 0, NULL, NULL);

    byte *prr = (byte *)malloc(32 * sigsLen);
    ret = clEnqueueReadBuffer(command_queue, pr_mem_obj, CL_TRUE, 0,
    		32 * sigsLen, prr, 0, NULL, NULL);

    ret = clReleaseMemObject(pr_mem_obj);
    ret = clReleaseMemObject(prea_mem_obj);
    ret = clReleaseMemObject(pparams_mem_obj);
    ret = clReleaseMemObject(pmc_mem_obj);

    QueryPerformanceCounter(&q3);

    // Check results
    pParams = ecmult_params;
    // pr = pr0;
    for (int i = 0; i < sigsLen; i++) { // TODO: Point at infinity
		unsigned char *pxrb = &prr[32 * i];
		secp256k1_num_t r2;
		secp256k1_num_set_bin(&r2, pxrb, 32);
		secp256k1_num_mod(&r2, &c->order);
		int ret = secp256k1_num_cmp(&pParams->r, &r2);
		prets[i] = ret == 0;

		pParams++;
    }

    QueryPerformanceCounter(&q4);
    unsigned long long tickerPerMs = ticksPerSecond.QuadPart / 1000;

    printf("prepare -> %lld\n", (q2.QuadPart - q1.QuadPart) / tickerPerMs);
    printf("compute -> %lld\n", (q3.QuadPart - q2.QuadPart) / tickerPerMs);
    printf("check   -> %lld\n", (q4.QuadPart - q3.QuadPart) / tickerPerMs);
    printf("total   -> %lld\n", (q4.QuadPart - q1.QuadPart) / tickerPerMs);
    printf("           %lf\n", (double)(q4.QuadPart - q1.QuadPart) / tickerPerMs / sigsLen);
    free(prr);
    free(ecmult_params);
    return prets;
}
