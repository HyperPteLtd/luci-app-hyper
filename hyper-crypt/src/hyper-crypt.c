// SPDX-License-Identifier: GPL-2.0-only
/*
 * hyper-crypt: tiny helper for luci-app-hyper.
 *
 * PPanel delivers the subscription (GET <subscribe_path>?token&country&client_public_key)
 * as an envelope encrypted with X25519 + HKDF-SHA256 + AES-256-GCM. ucode on
 * OpenWrt has none of these primitives and `openssl enc` cannot do AEAD, so this
 * helper does exactly two things with mbedtls:
 *
 *   hyper-crypt keygen
 *       -> {"private_key":"<b64url>","public_key":"<b64url>"}   (ephemeral X25519)
 *
 *   HYPER_CRYPT_PRIVATE_KEY=<b64url> hyper-crypt open <spk> <salt> <nonce> <info>  < data
 *       -> plaintext on stdout
 *
 * All binary arguments are base64 (standard or url-safe, padding optional).
 * `data` on stdin is base64 of ciphertext || 16-byte GCM tag.
 *
 * Matches the Hyper app (hyper-tunnel packages/app-client/src/subscribe/client.ts):
 *   - shared secret = X25519(client_private, spk), 32 bytes
 *   - key = HKDF-SHA256(ikm = shared, salt = envelope.salt (32 bytes), info = "subscribe-v1", L = 32)
 *   - AES-256-GCM, 12-byte nonce, no AAD, tag appended to the ciphertext
 *
 * Copyright (C) 2026 Hyper Pte. Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/gcm.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/md.h>
#include <mbedtls/platform_util.h>

#define KEY_LEN 32
#define TAG_LEN 16
#define HYPER_CRYPT_MAX_INPUT (8 * 1024 * 1024)

enum { EXIT_USAGE = 2, EXIT_DECODE = 3, EXIT_CRYPTO = 4 };

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context drbg;

static int rng_init(void)
{
	static const char pers[] = "hyper-crypt";

	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&drbg);
	return mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
				     (const unsigned char *)pers, sizeof(pers) - 1);
}

static void rng_free(void)
{
	mbedtls_ctr_drbg_free(&drbg);
	mbedtls_entropy_free(&entropy);
}

/* Decode standard or url-safe base64, with or without padding. Returns malloc'd buffer. */
static unsigned char *b64_decode(const char *in, size_t *out_len)
{
	size_t len = strlen(in), i, j = 0, olen = 0;
	char *norm = malloc(len + 4);
	unsigned char *out;

	if (!norm)
		return NULL;

	for (i = 0; i < len; i++) {
		char c = in[i];

		if (c == '\n' || c == '\r' || c == ' ' || c == '\t')
			continue;
		if (c == '-')
			c = '+';
		else if (c == '_')
			c = '/';
		norm[j++] = c;
	}
	while (j % 4)
		norm[j++] = '=';
	norm[j] = '\0';

	out = malloc(j / 4 * 3 + 1);
	if (!out || mbedtls_base64_decode(out, j / 4 * 3 + 1, &olen,
					  (const unsigned char *)norm, j) != 0) {
		free(norm);
		free(out);
		return NULL;
	}

	free(norm);
	*out_len = olen;
	return out;
}

/* Encode as url-safe base64 without padding into a static-sized caller buffer. */
static int b64url_encode(const unsigned char *in, size_t in_len, char *out, size_t out_size)
{
	size_t olen = 0, i;

	if (mbedtls_base64_encode((unsigned char *)out, out_size, &olen, in, in_len) != 0)
		return -1;

	for (i = 0; i < olen; i++) {
		if (out[i] == '+')
			out[i] = '-';
		else if (out[i] == '/')
			out[i] = '_';
	}
	while (olen && out[olen - 1] == '=')
		olen--;
	out[olen] = '\0';
	return 0;
}

static int cmd_keygen(void)
{
	mbedtls_ecp_group grp;
	mbedtls_mpi d;
	mbedtls_ecp_point q;
	unsigned char priv[KEY_LEN], pub[KEY_LEN];
	char priv_b64[64], pub_b64[64];
	size_t olen = 0;
	int ret = EXIT_CRYPTO;

	mbedtls_ecp_group_init(&grp);
	mbedtls_mpi_init(&d);
	mbedtls_ecp_point_init(&q);

	if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) != 0 ||
	    mbedtls_ecp_gen_keypair(&grp, &d, &q, mbedtls_ctr_drbg_random, &drbg) != 0 ||
	    mbedtls_mpi_write_binary_le(&d, priv, sizeof(priv)) != 0 ||
	    mbedtls_ecp_point_write_binary(&grp, &q, MBEDTLS_ECP_PF_UNCOMPRESSED,
					   &olen, pub, sizeof(pub)) != 0 ||
	    olen != KEY_LEN)
		goto out;

	if (b64url_encode(priv, sizeof(priv), priv_b64, sizeof(priv_b64)) ||
	    b64url_encode(pub, sizeof(pub), pub_b64, sizeof(pub_b64)))
		goto out;

	printf("{\"private_key\":\"%s\",\"public_key\":\"%s\"}\n", priv_b64, pub_b64);
	ret = 0;

out:
	mbedtls_platform_zeroize(priv, sizeof(priv));
	mbedtls_platform_zeroize(priv_b64, sizeof(priv_b64));
	mbedtls_ecp_point_free(&q);
	mbedtls_mpi_free(&d);
	mbedtls_ecp_group_free(&grp);
	return ret;
}

static char *read_stdin(size_t *len)
{
	size_t cap = 64 * 1024, n = 0, r;
	char *buf = malloc(cap + 1), *tmp;

	if (!buf)
		return NULL;

	while ((r = fread(buf + n, 1, cap - n, stdin)) > 0) {
		n += r;
		if (n == cap) {
			if (cap >= HYPER_CRYPT_MAX_INPUT) {
				free(buf);
				return NULL;
			}
			cap *= 2;
			tmp = realloc(buf, cap + 1);
			if (!tmp) {
				free(buf);
				return NULL;
			}
			buf = tmp;
		}
	}

	buf[n] = '\0';
	*len = n;
	return buf;
}

static int cmd_open(const char *spk_b64, const char *salt_b64, const char *nonce_b64, const char *info)
{
	const char *priv_b64 = getenv("HYPER_CRYPT_PRIVATE_KEY");
	unsigned char *priv = NULL, *spk = NULL, *salt = NULL, *nonce = NULL, *data = NULL, *plain = NULL;
	size_t priv_len = 0, spk_len = 0, salt_len = 0, nonce_len = 0, data_len = 0, in_len = 0;
	unsigned char shared[KEY_LEN], key[KEY_LEN];
	char *input = NULL;
	mbedtls_ecp_group grp;
	mbedtls_mpi d, z;
	mbedtls_ecp_point qp;
	mbedtls_gcm_context gcm;
	int ret = EXIT_DECODE;

	mbedtls_ecp_group_init(&grp);
	mbedtls_mpi_init(&d);
	mbedtls_mpi_init(&z);
	mbedtls_ecp_point_init(&qp);
	mbedtls_gcm_init(&gcm);

	if (!priv_b64) {
		fprintf(stderr, "HYPER_CRYPT_PRIVATE_KEY is not set\n");
		ret = EXIT_USAGE;
		goto out;
	}

	priv = b64_decode(priv_b64, &priv_len);
	spk = b64_decode(spk_b64, &spk_len);
	salt = b64_decode(salt_b64, &salt_len);
	nonce = b64_decode(nonce_b64, &nonce_len);
	input = read_stdin(&in_len);
	if (input)
		data = b64_decode(input, &data_len);

	if (!priv || priv_len != KEY_LEN || !spk || spk_len != KEY_LEN ||
	    !salt || !nonce || nonce_len == 0 || !data || data_len <= TAG_LEN) {
		fprintf(stderr, "invalid key, salt, nonce or data\n");
		goto out;
	}

	ret = EXIT_CRYPTO;
	if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) != 0 ||
	    mbedtls_mpi_read_binary_le(&d, priv, priv_len) != 0 ||
	    mbedtls_ecp_check_privkey(&grp, &d) != 0 ||
	    mbedtls_ecp_point_read_binary(&grp, &qp, spk, spk_len) != 0 ||
	    mbedtls_ecdh_compute_shared(&grp, &z, &qp, &d, mbedtls_ctr_drbg_random, &drbg) != 0 ||
	    mbedtls_mpi_write_binary_le(&z, shared, sizeof(shared)) != 0) {
		fprintf(stderr, "x25519 failed\n");
		goto out;
	}

	if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
			 salt, salt_len, shared, sizeof(shared),
			 (const unsigned char *)info, strlen(info), key, sizeof(key)) != 0) {
		fprintf(stderr, "hkdf failed\n");
		goto out;
	}

	plain = malloc(data_len - TAG_LEN + 1);
	if (!plain ||
	    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, KEY_LEN * 8) != 0 ||
	    mbedtls_gcm_auth_decrypt(&gcm, data_len - TAG_LEN, nonce, nonce_len, NULL, 0,
				     data + data_len - TAG_LEN, TAG_LEN, data, plain) != 0) {
		fprintf(stderr, "decryption failed\n");
		goto out;
	}

	fwrite(plain, 1, data_len - TAG_LEN, stdout);
	ret = 0;

out:
	mbedtls_platform_zeroize(shared, sizeof(shared));
	mbedtls_platform_zeroize(key, sizeof(key));
	if (priv)
		mbedtls_platform_zeroize(priv, priv_len);
	free(priv);
	free(spk);
	free(salt);
	free(nonce);
	free(data);
	free(input);
	free(plain);
	mbedtls_gcm_free(&gcm);
	mbedtls_ecp_point_free(&qp);
	mbedtls_mpi_free(&z);
	mbedtls_mpi_free(&d);
	mbedtls_ecp_group_free(&grp);
	return ret;
}

static int usage(void)
{
	fprintf(stderr,
		"usage: hyper-crypt keygen\n"
		"       HYPER_CRYPT_PRIVATE_KEY=<b64> hyper-crypt open <spk> <salt> <nonce> <info> < data\n");
	return EXIT_USAGE;
}

int main(int argc, char **argv)
{
	int ret;

	if (argc < 2)
		return usage();

	if (rng_init() != 0) {
		fprintf(stderr, "rng init failed\n");
		return EXIT_CRYPTO;
	}

	if (!strcmp(argv[1], "keygen") && argc == 2)
		ret = cmd_keygen();
	else if (!strcmp(argv[1], "open") && argc == 6)
		ret = cmd_open(argv[2], argv[3], argv[4], argv[5]);
	else
		ret = usage();

	rng_free();
	return ret;
}
