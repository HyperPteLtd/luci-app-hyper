// SPDX-License-Identifier: GPL-2.0-only
/*
 * Test-only counterpart of `hyper-crypt open`: plays the server side of the
 * PPanel /v2 envelope so the helper can be round-tripped on a PC.
 *
 *   seal <client_public_key_b64> <info> < plaintext
 *     -> {"version":2,"spk":"...","salt":"...","nonce":"...","data":"..."}
 *
 * Mirrors the assumptions in src/hyper-crypt.c (X25519, HKDF-SHA256 with the
 * envelope salt, AES-256-GCM with the tag appended, no AAD).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/entropy.h>
#include <mbedtls/gcm.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/md.h>

static void b64(const unsigned char *in, size_t len, char *out, size_t cap)
{
	size_t olen = 0;

	mbedtls_base64_encode((unsigned char *)out, cap, &olen, in, len);
	out[olen] = '\0';
}

int main(int argc, char **argv)
{
	mbedtls_entropy_context ent;
	mbedtls_ctr_drbg_context drbg;
	mbedtls_ecp_group grp;
	mbedtls_mpi d, z;
	mbedtls_ecp_point q, qc;
	mbedtls_gcm_context gcm;
	unsigned char client_pub[64], spk[32], shared[32], key[32], salt[32], nonce[12], tag[16];
	static unsigned char plain[1 << 20], ct[(1 << 20) + 16];
	static char out[(3 << 20)];
	char spk_b64[64], salt_b64[64], nonce_b64[64];
	size_t n, olen = 0, plen;

	if (argc != 3)
		return 2;

	if (mbedtls_base64_decode(client_pub, sizeof(client_pub), &n,
				  (const unsigned char *)argv[1], strlen(argv[1])) || n != 32)
		return 3;

	plen = fread(plain, 1, sizeof(plain), stdin);

	mbedtls_entropy_init(&ent);
	mbedtls_ctr_drbg_init(&drbg);
	mbedtls_ecp_group_init(&grp);
	mbedtls_mpi_init(&d);
	mbedtls_mpi_init(&z);
	mbedtls_ecp_point_init(&q);
	mbedtls_ecp_point_init(&qc);
	mbedtls_gcm_init(&gcm);

	if (mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &ent, NULL, 0) ||
	    mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) ||
	    mbedtls_ecp_gen_keypair(&grp, &d, &q, mbedtls_ctr_drbg_random, &drbg) ||
	    mbedtls_ecp_point_write_binary(&grp, &q, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, spk, sizeof(spk)) ||
	    mbedtls_ecp_point_read_binary(&grp, &qc, client_pub, 32) ||
	    mbedtls_ecdh_compute_shared(&grp, &z, &qc, &d, mbedtls_ctr_drbg_random, &drbg) ||
	    mbedtls_mpi_write_binary_le(&z, shared, sizeof(shared)) ||
	    mbedtls_ctr_drbg_random(&drbg, salt, sizeof(salt)) ||
	    mbedtls_ctr_drbg_random(&drbg, nonce, sizeof(nonce)) ||
	    mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, sizeof(salt),
			 shared, sizeof(shared), (const unsigned char *)argv[2], strlen(argv[2]),
			 key, sizeof(key)) ||
	    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) ||
	    mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plen, nonce, sizeof(nonce),
				      NULL, 0, plain, ct, sizeof(tag), tag))
		return 4;

	memcpy(ct + plen, tag, sizeof(tag));
	b64(spk, sizeof(spk), spk_b64, sizeof(spk_b64));
	b64(salt, sizeof(salt), salt_b64, sizeof(salt_b64));
	b64(nonce, sizeof(nonce), nonce_b64, sizeof(nonce_b64));
	b64(ct, plen + sizeof(tag), out, sizeof(out));

	printf("{\"version\":2,\"spk\":\"%s\",\"salt\":\"%s\",\"nonce\":\"%s\",\"data\":\"%s\"}\n",
	       spk_b64, salt_b64, nonce_b64, out);
	return 0;
}
