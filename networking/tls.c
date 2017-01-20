/*
 * Copyright (C) 2017 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config TLS
//config:	bool "tls (debugging)"
//config:	default n

//applet:IF_TLS(APPLET(tls, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_TLS) += tls.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_montgomery_reduce.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_mul_comba.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_sqr_comba.o
//kbuild:lib-$(CONFIG_TLS) += tls_rsa.o
//kbuild:lib-$(CONFIG_TLS) += tls_aes.o
////kbuild:lib-$(CONFIG_TLS) += tls_aes_gcm.o

//usage:#define tls_trivial_usage
//usage:       "HOST[:PORT]"
//usage:#define tls_full_usage "\n\n"

#include "tls.h"
//#include "common_bufsiz.h"

#define TLS_DEBUG      1
#define TLS_DEBUG_HASH 0
#define TLS_DEBUG_DER  0
#define TLS_DEBUG_FIXED_SECRETS 0

#if TLS_DEBUG
# define dbg(...) fprintf(stderr, __VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

#if TLS_DEBUG_DER
# define dbg_der(...) fprintf(stderr, __VA_ARGS__)
#else
# define dbg_der(...) ((void)0)
#endif

#if 0
# define dump_raw_out(...) dump_hex(__VA_ARGS__)
#else
# define dump_raw_out(...) ((void)0)
#endif

#if 0
# define dump_raw_in(...) dump_hex(__VA_ARGS__)
#else
# define dump_raw_in(...) ((void)0)
#endif

#define RECORD_TYPE_CHANGE_CIPHER_SPEC  20
#define RECORD_TYPE_ALERT               21
#define RECORD_TYPE_HANDSHAKE           22
#define RECORD_TYPE_APPLICATION_DATA    23

#define HANDSHAKE_HELLO_REQUEST         0
#define HANDSHAKE_CLIENT_HELLO          1
#define HANDSHAKE_SERVER_HELLO          2
#define HANDSHAKE_HELLO_VERIFY_REQUEST  3
#define HANDSHAKE_NEW_SESSION_TICKET    4
#define HANDSHAKE_CERTIFICATE           11
#define HANDSHAKE_SERVER_KEY_EXCHANGE   12
#define HANDSHAKE_CERTIFICATE_REQUEST   13
#define HANDSHAKE_SERVER_HELLO_DONE     14
#define HANDSHAKE_CERTIFICATE_VERIFY    15
#define HANDSHAKE_CLIENT_KEY_EXCHANGE   16
#define HANDSHAKE_FINISHED              20

#define SSL_NULL_WITH_NULL_NULL                 0x0000
#define SSL_RSA_WITH_NULL_MD5                   0x0001
#define SSL_RSA_WITH_NULL_SHA                   0x0002
#define SSL_RSA_WITH_RC4_128_MD5                0x0004
#define SSL_RSA_WITH_RC4_128_SHA                0x0005
#define SSL_RSA_WITH_3DES_EDE_CBC_SHA           0x000A  /* 10 */
#define TLS_RSA_WITH_AES_128_CBC_SHA            0x002F  /* 47 */
#define TLS_RSA_WITH_AES_256_CBC_SHA            0x0035  /* 53 */
#define TLS_RSA_WITH_NULL_SHA256                0x003B  /* 59 */

#define TLS_EMPTY_RENEGOTIATION_INFO_SCSV       0x00FF

#define TLS_RSA_WITH_IDEA_CBC_SHA               0x0007  /* 7 */
#define SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA       0x0016  /* 22 */
#define SSL_DH_anon_WITH_RC4_128_MD5            0x0018  /* 24 */
#define SSL_DH_anon_WITH_3DES_EDE_CBC_SHA       0x001B  /* 27 */
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA        0x0033  /* 51 */
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA        0x0039  /* 57 */
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA256     0x0067  /* 103 */
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA256     0x006B  /* 107 */
#define TLS_DH_anon_WITH_AES_128_CBC_SHA        0x0034  /* 52 */
#define TLS_DH_anon_WITH_AES_256_CBC_SHA        0x003A  /* 58 */
#define TLS_RSA_WITH_AES_128_CBC_SHA256         0x003C  /* 60 */
#define TLS_RSA_WITH_AES_256_CBC_SHA256         0x003D  /* 61 */
#define TLS_RSA_WITH_SEED_CBC_SHA               0x0096  /* 150 */
#define TLS_PSK_WITH_AES_128_CBC_SHA            0x008C  /* 140 */
#define TLS_PSK_WITH_AES_128_CBC_SHA256         0x00AE  /* 174 */
#define TLS_PSK_WITH_AES_256_CBC_SHA384         0x00AF  /* 175 */
#define TLS_PSK_WITH_AES_256_CBC_SHA            0x008D  /* 141 */
#define TLS_DHE_PSK_WITH_AES_128_CBC_SHA        0x0090  /* 144 */
#define TLS_DHE_PSK_WITH_AES_256_CBC_SHA        0x0091  /* 145 */
#define TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA     0xC004  /* 49156 */
#define TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA     0xC005  /* 49157 */
#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA    0xC009  /* 49161 */
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA    0xC00A  /* 49162 */
#define TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA     0xC012  /* 49170 */
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA      0xC013  /* 49171 */
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA      0xC014  /* 49172 */
#define TLS_ECDH_RSA_WITH_AES_128_CBC_SHA       0xC00E  /* 49166 */
#define TLS_ECDH_RSA_WITH_AES_256_CBC_SHA       0xC00F  /* 49167 */
#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 0xC023  /* 49187 */
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 0xC024  /* 49188 */
#define TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256  0xC025  /* 49189 */
#define TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384  0xC026  /* 49190 */
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256   0xC027  /* 49191 */
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384   0xC028  /* 49192 */
#define TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256    0xC029  /* 49193 */
#define TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384    0xC02A  /* 49194 */

// RFC 5288 "AES Galois Counter Mode (GCM) Cipher Suites for TLS"
#define TLS_RSA_WITH_AES_128_GCM_SHA256         0x009C  /* 156 */
#define TLS_RSA_WITH_AES_256_GCM_SHA384         0x009D  /* 157 */
#define TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 0xC02B  /* 49195 */
#define TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 0xC02C  /* 49196 */
#define TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256  0xC02D  /* 49197 */
#define TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384  0xC02E  /* 49198 */
#define TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256   0xC02F  /* 49199 */
#define TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384   0xC030  /* 49200 */
#define TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256    0xC031  /* 49201 */
#define TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384    0xC032  /* 49202 */

//Tested against kernel.org:
//TLS 1.1
//#define TLS_MAJ 3
//#define TLS_MIN 2
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA // ok, recvs SERVER_KEY_EXCHANGE
//TLS 1.2
#define TLS_MAJ 3
#define TLS_MIN 3
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA // ok, recvs SERVER_KEY_EXCHANGE *** matrixssl uses this on my box
//#define CIPHER_ID TLS_RSA_WITH_AES_256_CBC_SHA256 // ok, no SERVER_KEY_EXCHANGE
// All GCMs:
//#define CIPHER_ID TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 // ok, recvs SERVER_KEY_EXCHANGE
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
//#define CIPHER_ID TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384
//#define CIPHER_ID TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384
//#define CIPHER_ID TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_RSA_WITH_AES_256_GCM_SHA384 // ok, no SERVER_KEY_EXCHANGE
//#define CIPHER_ID TLS_RSA_WITH_AES_128_GCM_SHA256 // ok, no SERVER_KEY_EXCHANGE *** select this?
//#define CIPHER_ID TLS_DH_anon_WITH_AES_256_CBC_SHA // SSL_ALERT_HANDSHAKE_FAILURE
//^^^^^^^^^^^^^^^^^^^^^^^ (tested b/c this one doesn't req server certs... no luck)
//test TLS_RSA_WITH_AES_128_CBC_SHA, in TLS 1.2 it's mandated to be always supported

// works against "openssl s_server -cipher NULL"
// and against wolfssl-3.9.10-stable/examples/server/server.c:
//#define CIPHER_ID TLS_RSA_WITH_NULL_SHA256 // for testing (does everything except encrypting)
// works against wolfssl-3.9.10-stable/examples/server/server.c
#define CIPHER_ID TLS_RSA_WITH_AES_256_CBC_SHA256 // ok, no SERVER_KEY_EXCHANGE

enum {
	SHA256_INSIZE = 64,
	SHA256_OUTSIZE = 32,

	AES_BLOCKSIZE = 16,
	AES128_KEYSIZE = 16,
	AES256_KEYSIZE = 32,

	RSA_PREMASTER_SIZE = 48,

	RECHDR_LEN = 5,

	MAX_TLS_RECORD = (1 << 14),
	/* 8 = 3+5. 3 extra bytes result in record data being 32-bit aligned */
	OUTBUF_PFX = 8 + AES_BLOCKSIZE, /* header + IV */
	OUTBUF_SFX = SHA256_OUTSIZE + AES_BLOCKSIZE, /* MAC + padding */
	MAX_OUTBUF = MAX_TLS_RECORD - OUTBUF_PFX - OUTBUF_SFX,
};

struct record_hdr {
	uint8_t type;
	uint8_t proto_maj, proto_min;
	uint8_t len16_hi, len16_lo;
};

typedef struct tls_state {
	int fd;

	psRsaKey_t server_rsa_pub_key;

	sha256_ctx_t handshake_sha256_ctx;

	uint8_t client_and_server_rand32[2 * 32];
	uint8_t master_secret[48];

	uint8_t encrypt_on_write;
	int     min_encrypted_len_on_read;
	uint8_t client_write_MAC_key[SHA256_OUTSIZE];
	uint8_t server_write_MAC_key[SHA256_OUTSIZE];
	uint8_t client_write_key[AES256_KEYSIZE];
	uint8_t server_write_key[AES256_KEYSIZE];

	// RFC 5246
	// sequence number
	//   Each connection state contains a sequence number, which is
	//   maintained separately for read and write states.  The sequence
	//   number MUST be set to zero whenever a connection state is made the
	//   active state.  Sequence numbers are of type uint64 and may not
	//   exceed 2^64-1.
	uint64_t write_seq64_be;

	int outbuf_size;
	uint8_t *outbuf;

	// RFC 5246
	// | 6.2.1. Fragmentation
	// |  The record layer fragments information blocks into TLSPlaintext
	// |  records carrying data in chunks of 2^14 bytes or less.  Client
	// |  message boundaries are not preserved in the record layer (i.e.,
	// |  multiple client messages of the same ContentType MAY be coalesced
	// |  into a single TLSPlaintext record, or a single message MAY be
	// |  fragmented across several records)
	// |...
	// |  length
	// |    The length (in bytes) of the following TLSPlaintext.fragment.
	// |    The length MUST NOT exceed 2^14.
	// |...
	// | 6.2.2. Record Compression and Decompression
	// |...
	// |  Compression must be lossless and may not increase the content length
	// |  by more than 1024 bytes.  If the decompression function encounters a
	// |  TLSCompressed.fragment that would decompress to a length in excess of
	// |  2^14 bytes, it MUST report a fatal decompression failure error.
	// |...
	// |  length
	// |    The length (in bytes) of the following TLSCompressed.fragment.
	// |    The length MUST NOT exceed 2^14 + 1024.
	//
	// Since our buffer also contains 5-byte headers, make it a bit bigger:
	int insize;
	int tail;
//needed?
	uint64_t align____;
	uint8_t inbuf[20*1024];
} tls_state_t;


static unsigned get24be(const uint8_t *p)
{
	return 0x100*(0x100*p[0] + p[1]) + p[2];
}

#if TLS_DEBUG
static void dump_hex(const char *fmt, const void *vp, int len)
{
	char hexbuf[32 * 1024 + 4];
	const uint8_t *p = vp;

	bin2hex(hexbuf, (void*)p, len)[0] = '\0';
	dbg(fmt, hexbuf);
}

static void dump_tls_record(const void *vp, int len)
{
	const uint8_t *p = vp;

	while (len > 0) {
		unsigned xhdr_len;
		if (len < RECHDR_LEN) {
			dump_hex("< |%s|\n", p, len);
			return;
		}
		xhdr_len = 0x100*p[3] + p[4];
		dbg("< hdr_type:%u ver:%u.%u len:%u", p[0], p[1], p[2], xhdr_len);
		p += RECHDR_LEN;
		len -= RECHDR_LEN;
		if (len >= 4 && p[-RECHDR_LEN] == RECORD_TYPE_HANDSHAKE) {
			unsigned len24 = get24be(p + 1);
			dbg(" type:%u len24:%u", p[0], len24);
		}
		if (xhdr_len > len)
			xhdr_len = len;
		dump_hex(" |%s|\n", p, xhdr_len);
		p += xhdr_len;
		len -= xhdr_len;
	}
}
#else
# define dump_hex(...) ((void)0)
# define dump_tls_record(...) ((void)0)
#endif

void tls_get_random(void *buf, unsigned len)
{
	if (len != open_read_close("/dev/urandom", buf, len))
		xfunc_die();
}

//TODO rename this to sha256_hash, and sha256_hash -> sha256_update
static void hash_sha256(uint8_t out[SHA256_OUTSIZE], const void *data, unsigned size)
{
	sha256_ctx_t ctx;
	sha256_begin(&ctx);
	sha256_hash(&ctx, data, size);
	sha256_end(&ctx, out);
}

/* Nondestructively see the current hash value */
static void sha256_peek(sha256_ctx_t *ctx, void *buffer)
{
	sha256_ctx_t ctx_copy = *ctx;
        sha256_end(&ctx_copy, buffer);
}

#if TLS_DEBUG_HASH
static void sha256_hash_dbg(const char *fmt, sha256_ctx_t *ctx, const void *buffer, size_t len)
{
        uint8_t h[SHA256_OUTSIZE];

	sha256_hash(ctx, buffer, len);
	dump_hex(fmt, buffer, len);
	dbg(" (%u) ", (int)len);
	sha256_peek(ctx, h);
	dump_hex("%s\n", h, SHA256_OUTSIZE);
}
#else
# define sha256_hash_dbg(fmt, ctx, buffer, len) \
         sha256_hash(ctx, buffer, len)
#endif

// RFC 2104
// HMAC(key, text) based on a hash H (say, sha256) is:
// ipad = [0x36 x INSIZE]
// opad = [0x5c x INSIZE]
// HMAC(key, text) = H((key XOR opad) + H((key XOR ipad) + text))
//
// H(key XOR opad) and H(key XOR ipad) can be precomputed
// if we often need HMAC hmac with the same key.
//
// text is often given in disjoint pieces.
static void hmac_sha256_precomputed_v(uint8_t out[SHA256_OUTSIZE],
		sha256_ctx_t *hashed_key_xor_ipad,
		sha256_ctx_t *hashed_key_xor_opad,
		va_list va)
{
	uint8_t *text;

	/* hashed_key_xor_ipad contains unclosed "H((key XOR ipad) +" state */
	/* hashed_key_xor_opad contains unclosed "H((key XOR opad) +" state */

	/* calculate out = H((key XOR ipad) + text) */
	while ((text = va_arg(va, uint8_t*)) != NULL) {
		unsigned text_size = va_arg(va, unsigned);
		sha256_hash(hashed_key_xor_ipad, text, text_size);
	}
	sha256_end(hashed_key_xor_ipad, out);

	/* out = H((key XOR opad) + out) */
	sha256_hash(hashed_key_xor_opad, out, SHA256_OUTSIZE);
	sha256_end(hashed_key_xor_opad, out);
}

static void hmac_sha256(uint8_t out[SHA256_OUTSIZE], uint8_t *key, unsigned key_size, ...)
{
	sha256_ctx_t hashed_key_xor_ipad;
	sha256_ctx_t hashed_key_xor_opad;
	uint8_t key_xor_ipad[SHA256_INSIZE];
	uint8_t key_xor_opad[SHA256_INSIZE];
	uint8_t tempkey[SHA256_OUTSIZE];
	va_list va;
	int i;

	va_start(va, key_size);

	// "The authentication key can be of any length up to INSIZE, the
	// block length of the hash function.  Applications that use keys longer
	// than INSIZE bytes will first hash the key using H and then use the
	// resultant OUTSIZE byte string as the actual key to HMAC."
	if (key_size > SHA256_INSIZE) {
		hash_sha256(tempkey, key, key_size);
		key = tempkey;
		key_size = SHA256_OUTSIZE;
	}

	for (i = 0; i < key_size; i++) {
		key_xor_ipad[i] = key[i] ^ 0x36;
		key_xor_opad[i] = key[i] ^ 0x5c;
	}
	for (; i < SHA256_INSIZE; i++) {
		key_xor_ipad[i] = 0x36;
		key_xor_opad[i] = 0x5c;
	}
	sha256_begin(&hashed_key_xor_ipad);
	sha256_hash(&hashed_key_xor_ipad, key_xor_ipad, SHA256_INSIZE);
	sha256_begin(&hashed_key_xor_opad);
	sha256_hash(&hashed_key_xor_opad, key_xor_opad, SHA256_INSIZE);

	hmac_sha256_precomputed_v(out, &hashed_key_xor_ipad, &hashed_key_xor_opad, va);
	va_end(va);
}

// RFC 5246:
// 5.  HMAC and the Pseudorandom Function
//...
// In this section, we define one PRF, based on HMAC.  This PRF with the
// SHA-256 hash function is used for all cipher suites defined in this
// document and in TLS documents published prior to this document when
// TLS 1.2 is negotiated.
//...
//    P_hash(secret, seed) = HMAC_hash(secret, A(1) + seed) +
//                           HMAC_hash(secret, A(2) + seed) +
//                           HMAC_hash(secret, A(3) + seed) + ...
// where + indicates concatenation.
// A() is defined as:
//    A(0) = seed
//    A(1) = HMAC_hash(secret, A(0)) = HMAC_hash(secret, seed)
//    A(i) = HMAC_hash(secret, A(i-1))
// P_hash can be iterated as many times as necessary to produce the
// required quantity of data.  For example, if P_SHA256 is being used to
// create 80 bytes of data, it will have to be iterated three times
// (through A(3)), creating 96 bytes of output data; the last 16 bytes
// of the final iteration will then be discarded, leaving 80 bytes of
// output data.
//
// TLS's PRF is created by applying P_hash to the secret as:
//
//    PRF(secret, label, seed) = P_<hash>(secret, label + seed)
//
// The label is an ASCII string.
static void prf_hmac_sha256(
		uint8_t *outbuf, unsigned outbuf_size,
		uint8_t *secret, unsigned secret_size,
		const char *label,
		uint8_t *seed, unsigned seed_size)
{
	uint8_t a[SHA256_OUTSIZE];
	uint8_t *out_p = outbuf;
	unsigned label_size = strlen(label);

	/* In P_hash() calculation, "seed" is "label + seed": */
#define SEED   label, label_size, seed, seed_size
#define SECRET secret, secret_size
#define A      a, (int)(sizeof(a))

	/* A(1) = HMAC_hash(secret, seed) */
	hmac_sha256(a, SECRET, SEED, NULL);
//TODO: convert hmac_sha256 to precomputed

	for(;;) {
		/* HMAC_hash(secret, A(1) + seed) */
		if (outbuf_size <= SHA256_OUTSIZE) {
			/* Last, possibly incomplete, block */
			/* (use a[] as temp buffer) */
			hmac_sha256(a, SECRET, A, SEED, NULL);
			memcpy(out_p, a, outbuf_size);
			return;
		}
		/* Not last block. Store directly to result buffer */
		hmac_sha256(out_p, SECRET, A, SEED, NULL);
		out_p += SHA256_OUTSIZE;
		outbuf_size -= SHA256_OUTSIZE;
		/* A(2) = HMAC_hash(secret, A(1)) */
		hmac_sha256(a, SECRET, A, NULL);
	}
#undef A
#undef SECRET
#undef SEED
}

static tls_state_t *new_tls_state(void)
{
	tls_state_t *tls = xzalloc(sizeof(*tls));
	tls->fd = -1;
	sha256_begin(&tls->handshake_sha256_ctx);
	return tls;
}

static void tls_error_die(tls_state_t *tls)
{
	dump_tls_record(tls->inbuf, tls->insize + tls->tail);
	bb_error_msg_and_die("TODO: useful diagnostic about %p", tls);
}

static void tls_free_outbuf(tls_state_t *tls)
{
	free(tls->outbuf);
	tls->outbuf_size = 0;
	tls->outbuf = NULL;
}

static void *tls_get_outbuf(tls_state_t *tls, int len)
{
	if (len > MAX_OUTBUF)
		xfunc_die();
	if (tls->outbuf_size < len + OUTBUF_PFX + OUTBUF_SFX) {
		tls->outbuf_size = len + OUTBUF_PFX + OUTBUF_SFX;
		tls->outbuf = xrealloc(tls->outbuf, tls->outbuf_size);
	}
	return tls->outbuf + OUTBUF_PFX;
}

static void xwrite_encrypted(tls_state_t *tls, unsigned size, unsigned type)
{
	uint8_t *buf = tls->outbuf + OUTBUF_PFX;
	struct record_hdr *xhdr;
	uint8_t padding_length;

	xhdr = (void*)(buf - RECHDR_LEN);
	if (CIPHER_ID != TLS_RSA_WITH_NULL_SHA256)
		xhdr = (void*)(buf - RECHDR_LEN - AES_BLOCKSIZE); /* place for IV */

	xhdr->type = type;
	xhdr->proto_maj = TLS_MAJ;
	xhdr->proto_min = TLS_MIN;
	/* fake unencrypted record header len for MAC calculation */
	xhdr->len16_hi = size >> 8;
	xhdr->len16_lo = size & 0xff;

	/* Calculate MAC signature */
//TODO: convert hmac_sha256 to precomputed
	hmac_sha256(buf + size,
			tls->client_write_MAC_key, sizeof(tls->client_write_MAC_key),
			&tls->write_seq64_be, sizeof(tls->write_seq64_be),
			xhdr, RECHDR_LEN,
			buf, size,
			NULL);
	tls->write_seq64_be = SWAP_BE64(1 + SWAP_BE64(tls->write_seq64_be));

	size += SHA256_OUTSIZE;

	// RFC 5246
	// 6.2.3.1.  Null or Standard Stream Cipher
	//
	// Stream ciphers (including BulkCipherAlgorithm.null; see Appendix A.6)
	// convert TLSCompressed.fragment structures to and from stream
	// TLSCiphertext.fragment structures.
	//
	//    stream-ciphered struct {
	//        opaque content[TLSCompressed.length];
	//        opaque MAC[SecurityParameters.mac_length];
	//    } GenericStreamCipher;
	//
	// The MAC is generated as:
	//    MAC(MAC_write_key, seq_num +
	//                          TLSCompressed.type +
	//                          TLSCompressed.version +
	//                          TLSCompressed.length +
	//                          TLSCompressed.fragment);
	// where "+" denotes concatenation.
	// seq_num
	//    The sequence number for this record.
	// MAC
	//    The MAC algorithm specified by SecurityParameters.mac_algorithm.
	//
	// Note that the MAC is computed before encryption.  The stream cipher
	// encrypts the entire block, including the MAC.
	//...
	// Appendix C.  Cipher Suite Definitions
	//...
	// MAC       Algorithm    mac_length  mac_key_length
	// --------  -----------  ----------  --------------
	// SHA       HMAC-SHA1       20            20
	// SHA256    HMAC-SHA256     32            32
	if (CIPHER_ID == TLS_RSA_WITH_NULL_SHA256) {
		/* No encryption, only signing */
		xhdr->len16_hi = size >> 8;
		xhdr->len16_lo = size & 0xff;
		dump_raw_out(">> %s\n", xhdr, RECHDR_LEN + size);
		xwrite(tls->fd, xhdr, RECHDR_LEN + size);
		dbg("wrote %u bytes (NULL crypt, SHA256 hash)\n", size);
		return;
	}

	// 6.2.3.2.  CBC Block Cipher
	// For block ciphers (such as 3DES or AES), the encryption and MAC
	// functions convert TLSCompressed.fragment structures to and from block
	// TLSCiphertext.fragment structures.
	//    struct {
	//        opaque IV[SecurityParameters.record_iv_length];
	//        block-ciphered struct {
	//            opaque content[TLSCompressed.length];
	//            opaque MAC[SecurityParameters.mac_length];
	//            uint8 padding[GenericBlockCipher.padding_length];
	//            uint8 padding_length;
	//        };
	//    } GenericBlockCipher;
	//...
	// IV
	//    The Initialization Vector (IV) SHOULD be chosen at random, and
	//    MUST be unpredictable.  Note that in versions of TLS prior to 1.1,
	//    there was no IV field (...).  For block ciphers, the IV length is
	//    of length SecurityParameters.record_iv_length, which is equal to the
	//    SecurityParameters.block_size.
	// padding
	//    Padding that is added to force the length of the plaintext to be
	//    an integral multiple of the block cipher's block length.
	// padding_length
	//    The padding length MUST be such that the total size of the
	//    GenericBlockCipher structure is a multiple of the cipher's block
	//    length.  Legal values range from zero to 255, inclusive.
	//...
	// Appendix C.  Cipher Suite Definitions
	//...
	//                         Key      IV   Block
	// Cipher        Type    Material  Size  Size
	// ------------  ------  --------  ----  -----
	// AES_128_CBC   Block      16      16     16
	// AES_256_CBC   Block      32      16     16

	/* Build IV+content+MAC+padding in outbuf */
	tls_get_random(buf - AES_BLOCKSIZE, AES_BLOCKSIZE); /* IV */
	dbg("before crypt: 5 hdr + %u data + %u hash bytes\n", size, SHA256_OUTSIZE);
	// RFC is talking nonsense:
	//    Padding that is added to force the length of the plaintext to be
	//    an integral multiple of the block cipher's block length.
	// WRONG. _padding+padding_length_, not just _padding_,
	// pads the data.
	// IOW: padding_length is the last byte of padding[] array,
	// contrary to what RFC depicts.
	//
	// What actually happens is that there is always padding.
	// If you need one byte to reach BLOCKSIZE, this byte is 0x00.
	// If you need two bytes, they are both 0x01.
	// If you need three, they are 0x02,0x02,0x02. And so on.
	// If you need no bytes to reach BLOCKSIZE, you have to pad a full
	// BLOCKSIZE with bytes of value (BLOCKSIZE-1).
	// It's ok to have more than minimum padding, but we do minimum.
	padding_length = (~size) & (AES_BLOCKSIZE - 1);
	do {
		buf[size++] = padding_length;              /* padding */
	} while ((size & (AES_BLOCKSIZE - 1)) != 0);

	/* Encrypt content+MAC+padding in place */
	{
		psCipherContext_t ctx;
		psAesInit(&ctx, buf - AES_BLOCKSIZE, /* IV */
			tls->client_write_key, sizeof(tls->client_write_key)
		);
		psAesEncrypt(&ctx,
			buf, /* plaintext */
			buf, /* ciphertext */
			size
		);
	}

	/* Write out */
	dbg("writing 5 + %u IV + %u encrypted bytes, padding_length:0x%02x\n",
			AES_BLOCKSIZE, size, padding_length);
	size += AES_BLOCKSIZE;     /* + IV */
	xhdr->len16_hi = size >> 8;
	xhdr->len16_lo = size & 0xff;
	dump_raw_out(">> %s\n", xhdr, RECHDR_LEN + size);
	xwrite(tls->fd, xhdr, RECHDR_LEN + size);
	dbg("wrote %u bytes\n", (int)RECHDR_LEN + size);
}

static void xwrite_and_update_handshake_hash(tls_state_t *tls, unsigned size)
{
	if (!tls->encrypt_on_write) {
		uint8_t *buf = tls->outbuf + OUTBUF_PFX;
		struct record_hdr *xhdr = (void*)(buf - RECHDR_LEN);

		xhdr->type = RECORD_TYPE_HANDSHAKE;
		xhdr->proto_maj = TLS_MAJ;
		xhdr->proto_min = TLS_MIN;
		xhdr->len16_hi = size >> 8;
		xhdr->len16_lo = size & 0xff;
		dump_raw_out(">> %s\n", xhdr, RECHDR_LEN + size);
		xwrite(tls->fd, xhdr, RECHDR_LEN + size);
		dbg("wrote %u bytes\n", (int)RECHDR_LEN + size);
		/* Handshake hash does not include record headers */
		sha256_hash_dbg(">> sha256:%s", &tls->handshake_sha256_ctx, buf, size);
		return;
	}
	xwrite_encrypted(tls, size, RECORD_TYPE_HANDSHAKE);
}

static int tls_has_buffered_record(tls_state_t *tls)
{
	int buffered = tls->tail;
	struct record_hdr *xhdr;
	int rec_size;

	if (buffered < RECHDR_LEN)
		return 0;
	xhdr = (void*)(tls->inbuf + tls->insize);
	rec_size = RECHDR_LEN + (0x100 * xhdr->len16_hi + xhdr->len16_lo);
	if (buffered < rec_size)
		return 0;
	return rec_size;
}

static int tls_xread_record(tls_state_t *tls)
{
	struct record_hdr *xhdr;
	int sz;
	int total;
	int target;

 again:
	dbg("insize:%u tail:%u\n", tls->insize, tls->tail);
	total = tls->tail;
	if (total != 0) {
		memmove(tls->inbuf, tls->inbuf + tls->insize, total);
		//dbg("<< remaining at %d [%d] ", tls->insize, total);
		//dump_raw_in("<< %s\n", tls->inbuf, total);
	}
	errno = 0;
	target = sizeof(tls->inbuf);
	for (;;) {
		if (total >= RECHDR_LEN && target == sizeof(tls->inbuf)) {
			xhdr = (void*)tls->inbuf;
			target = RECHDR_LEN + (0x100 * xhdr->len16_hi + xhdr->len16_lo);
			if (target >= sizeof(tls->inbuf)) {
				/* malformed input (too long): yell and die */
				tls->tail = 0;
				tls->insize = total;
				tls_error_die(tls);
			}
			/* can also check type/proto_maj/proto_min here */
			dbg("xhdr type:%d ver:%d.%d len:%d\n",
				xhdr->type, xhdr->proto_maj, xhdr->proto_min,
				0x100 * xhdr->len16_hi + xhdr->len16_lo
			);
		}
		/* if total >= target, we have a full packet (and possibly more)... */
		if (total - target >= 0)
			break;
		sz = safe_read(tls->fd, tls->inbuf + total, sizeof(tls->inbuf) - total);
		if (sz <= 0) {
			if (sz == 0 && total == 0) {
				/* "Abrupt" EOF, no TLS shutdown (seen from kernel.org) */
				dbg("EOF (without TLS shutdown) from peer\n");
				tls->tail = 0;
				goto end;
			}
			bb_perror_msg_and_die("short read, have only %d", total);
		}
		dump_raw_in("<< %s\n", tls->inbuf + total, sz);
		total += sz;
	}
	tls->tail = total - target;
	tls->insize = target;
	//dbg("<< stashing at %d [%d] ", tls->insize, tls->tail);
	//dump_hex("<< %s\n", tls->inbuf + tls->insize, tls->tail);

	sz = target - RECHDR_LEN;

	/* Needs to be decrypted? */
	if (tls->min_encrypted_len_on_read > SHA256_OUTSIZE) {
		psCipherContext_t ctx;
		uint8_t *p = tls->inbuf + RECHDR_LEN;
		int padding_len;

		if (sz & (AES_BLOCKSIZE-1)
		 || sz < tls->min_encrypted_len_on_read
		) {
			bb_error_msg_and_die("bad encrypted len:%u", sz);
		}
		/* Decrypt content+MAC+padding in place */
		psAesInit(&ctx, p, /* IV */
			tls->server_write_key, sizeof(tls->server_write_key)
		);
		psAesDecrypt(&ctx,
			p + AES_BLOCKSIZE, /* ciphertext */
			p + AES_BLOCKSIZE, /* plaintext */
			sz - AES_BLOCKSIZE
		);
		padding_len = p[sz - 1];
		dbg("encrypted size:%u type:0x%02x padding_length:0x%02x\n", sz, p[AES_BLOCKSIZE], padding_len);
		padding_len++;
		sz -= AES_BLOCKSIZE + SHA256_OUTSIZE + padding_len;
		if (sz < 0) {
			bb_error_msg_and_die("bad padding size:%u", padding_len);
		}
		if (sz != 0) {
			/* Skip IV */
			memmove(tls->inbuf + RECHDR_LEN, tls->inbuf + RECHDR_LEN + AES_BLOCKSIZE, sz);
		}
	} else {
		/* if nonzero, then it's TLS_RSA_WITH_NULL_SHA256: drop MAC */
		/* else: no encryption yet on input, subtract zero = NOP */
		sz -= tls->min_encrypted_len_on_read;
	}

	//dump_hex("<< %s\n", tls->inbuf, RECHDR_LEN + sz);

	xhdr = (void*)tls->inbuf;
	if (xhdr->type == RECORD_TYPE_ALERT && sz >= 2) {
		uint8_t *p = tls->inbuf + RECHDR_LEN;
		dbg("ALERT size:%d level:%d description:%d\n", sz, p[0], p[1]);
		if (p[0] == 1) { /*warning */
			if (p[1] == 0) { /* warning, close_notify: EOF */
				dbg("EOF (TLS encoded) from peer\n");
				sz = 0;
				goto end;
			}
			/* discard it, get next record */
			goto again;
		}
		/* p[0] == 1: fatal error, others: not defined in protocol */
		sz = 0;
		goto end;
	}

	/* RFC 5246 is not saying it explicitly, but sha256 hash
	 * in our FINISHED record must include data of incoming packets too!
	 */
	if (tls->inbuf[0] == RECORD_TYPE_HANDSHAKE) {
		sha256_hash_dbg("<< sha256:%s", &tls->handshake_sha256_ctx, tls->inbuf + RECHDR_LEN, sz);
	}
 end:
	dbg("got block len:%u\n", sz);
	return sz;
}

/*
 * DER parsing routines
 */
static unsigned get_der_len(uint8_t **bodyp, uint8_t *der, uint8_t *end)
{
	unsigned len, len1;

	if (end - der < 2)
		xfunc_die();
//	if ((der[0] & 0x1f) == 0x1f) /* not single-byte item code? */
//		xfunc_die();

	len = der[1]; /* maybe it's short len */
	if (len >= 0x80) {
		/* no, it's long */

		if (len == 0x80 || end - der < (int)(len - 0x7e)) {
			/* 0x80 is "0 bytes of len", invalid DER: must use short len if can */
			/* need 3 or 4 bytes for 81, 82 */
			xfunc_die();
		}

		len1 = der[2]; /* if (len == 0x81) it's "ii 81 xx", fetch xx */
		if (len > 0x82) {
			/* >0x82 is "3+ bytes of len", should not happen realistically */
			xfunc_die();
		}
		if (len == 0x82) { /* it's "ii 82 xx yy" */
			len1 = 0x100*len1 + der[3];
			der += 1; /* skip [yy] */
		}
		der += 1; /* skip [xx] */
		len = len1;
//		if (len < 0x80)
//			xfunc_die(); /* invalid DER: must use short len if can */
	}
	der += 2; /* skip [code]+[1byte] */

	if (end - der < (int)len)
		xfunc_die();
	*bodyp = der;

	return len;
}

static uint8_t *enter_der_item(uint8_t *der, uint8_t **endp)
{
	uint8_t *new_der;
	unsigned len = get_der_len(&new_der, der, *endp);
	dbg_der("entered der @%p:0x%02x len:%u inner_byte @%p:0x%02x\n", der, der[0], len, new_der, new_der[0]);
	/* Move "end" position to cover only this item */
	*endp = new_der + len;
	return new_der;
}

static uint8_t *skip_der_item(uint8_t *der, uint8_t *end)
{
	uint8_t *new_der;
	unsigned len = get_der_len(&new_der, der, end);
	/* Skip body */
	new_der += len;
	dbg_der("skipped der 0x%02x, next byte 0x%02x\n", der[0], new_der[0]);
	return new_der;
}

static void der_binary_to_pstm(pstm_int *pstm_n, uint8_t *der, uint8_t *end)
{
	uint8_t *bin_ptr;
	unsigned len = get_der_len(&bin_ptr, der, end);

	dbg_der("binary bytes:%u, first:0x%02x\n", len, bin_ptr[0]);
	pstm_init_for_read_unsigned_bin(/*pool:*/ NULL, pstm_n, len);
	pstm_read_unsigned_bin(pstm_n, bin_ptr, len);
	//return bin + len;
}

static void find_key_in_der_cert(tls_state_t *tls, uint8_t *der, int len)
{
/* Certificate is a DER-encoded data structure. Each DER element has a length,
 * which makes it easy to skip over large compound elements of any complexity
 * without parsing them. Example: partial decode of kernel.org certificate:
 *  SEQ 0x05ac/1452 bytes (Certificate): 308205ac
 *    SEQ 0x0494/1172 bytes (tbsCertificate): 30820494
 *      [ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | 0] 3 bytes: a003
 *        INTEGER (version): 0201 02
 *      INTEGER 0x11 bytes (serialNumber): 0211 00 9f85bf664b0cddafca508679501b2be4
 *      //^^^^^^note: matrixSSL also allows [ASN_CONTEXT_SPECIFIC | ASN_PRIMITIVE | 2] = 0x82 type
 *      SEQ 0x0d bytes (signatureAlgo): 300d
 *        OID 9 bytes: 0609 2a864886f70d01010b (OID_SHA256_RSA_SIG 42.134.72.134.247.13.1.1.11)
 *        NULL: 0500
 *      SEQ 0x5f bytes (issuer): 305f
 *        SET 11 bytes: 310b
 *          SEQ 9 bytes: 3009
 *            OID 3 bytes: 0603 550406
 *            Printable string "FR": 1302 4652
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 550408
 *            Printable string "Paris": 1305 5061726973
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 550407
 *            Printable string "Paris": 1305 5061726973
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 55040a
 *            Printable string "Gandi": 1305 47616e6469
 *        SET 32 bytes: 3120
 *          SEQ 30 bytes: 301e
 *            OID 3 bytes: 0603 550403
 *            Printable string "Gandi Standard SSL CA 2": 1317 47616e6469205374616e646172642053534c2043412032
 *      SEQ 30 bytes (validity): 301e
 *        TIME "161011000000Z": 170d 3136313031313030303030305a
 *        TIME "191011235959Z": 170d 3139313031313233353935395a
 *      SEQ 0x5b/91 bytes (subject): 305b //I did not decode this
 *          3121301f060355040b1318446f6d61696e20436f
 *          6e74726f6c2056616c6964617465643121301f06
 *          0355040b1318506f73697469766553534c204d75
 *          6c74692d446f6d61696e31133011060355040313
 *          0a6b65726e656c2e6f7267
 *      SEQ 0x01a2/418 bytes (subjectPublicKeyInfo): 308201a2
 *        SEQ 13 bytes (algorithm): 300d
 *          OID 9 bytes: 0609 2a864886f70d010101 (OID_RSA_KEY_ALG 42.134.72.134.247.13.1.1.1)
 *          NULL: 0500
 *        BITSTRING 0x018f/399 bytes (publicKey): 0382018f
 *          ????: 00
 *          //after the zero byte, it appears key itself uses DER encoding:
 *          SEQ 0x018a/394 bytes: 3082018a
 *            INTEGER 0x0181/385 bytes (modulus): 02820181
 *                  00b1ab2fc727a3bef76780c9349bf3
 *                  ...24 more blocks of 15 bytes each...
 *                  90e895291c6bc8693b65
 *            INTEGER 3 bytes (exponent): 0203 010001
 *      [ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | 0x3] 0x01e5 bytes (X509v3 extensions): a38201e5
 *        SEQ 0x01e1 bytes: 308201e1
 *        ...
 * Certificate is a sequence of three elements:
 *	tbsCertificate (SEQ)
 *	signatureAlgorithm (AlgorithmIdentifier)
 *	signatureValue (BIT STRING)
 *
 * In turn, tbsCertificate is a sequence of:
 *	version
 *	serialNumber
 *	signatureAlgo (AlgorithmIdentifier)
 *	issuer (Name, has complex structure)
 *	validity (Validity, SEQ of two Times)
 *	subject (Name)
 *	subjectPublicKeyInfo (SEQ)
 *	...
 *
 * subjectPublicKeyInfo is a sequence of:
 *	algorithm (AlgorithmIdentifier)
 *	publicKey (BIT STRING)
 *
 * We need Certificate.tbsCertificate.subjectPublicKeyInfo.publicKey
 */
	uint8_t *end = der + len;

	/* enter "Certificate" item: [der, end) will be only Cert */
	der = enter_der_item(der, &end);

	/* enter "tbsCertificate" item: [der, end) will be only tbsCert */
	der = enter_der_item(der, &end);

	/* skip up to subjectPublicKeyInfo */
	der = skip_der_item(der, end); /* version */
	der = skip_der_item(der, end); /* serialNumber */
	der = skip_der_item(der, end); /* signatureAlgo */
	der = skip_der_item(der, end); /* issuer */
	der = skip_der_item(der, end); /* validity */
	der = skip_der_item(der, end); /* subject */

	/* enter subjectPublicKeyInfo */
	der = enter_der_item(der, &end);
	{ /* check subjectPublicKeyInfo.algorithm */
		static const uint8_t expected[] = {
			0x30,0x0d, // SEQ 13 bytes
			0x06,0x09, 0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01, // OID RSA_KEY_ALG 42.134.72.134.247.13.1.1.1
			//0x05,0x00, // NULL
		};
		if (memcmp(der, expected, sizeof(expected)) != 0)
			bb_error_msg_and_die("not RSA key");
	}
	/* skip subjectPublicKeyInfo.algorithm */
	der = skip_der_item(der, end);
	/* enter subjectPublicKeyInfo.publicKey */
//	die_if_not_this_der_type(der, end, 0x03); /* must be BITSTRING */
	der = enter_der_item(der, &end);

	/* parse RSA key: */
//based on getAsnRsaPubKey(), pkcs1ParsePrivBin() is also of note
	dbg("key bytes:%u, first:0x%02x\n", (int)(end - der), der[0]);
	if (end - der < 14) xfunc_die();
	/* example format:
	 * ignore bits: 00
	 * SEQ 0x018a/394 bytes: 3082018a
	 *   INTEGER 0x0181/385 bytes (modulus): 02820181 XX...XXX
	 *   INTEGER 3 bytes (exponent): 0203 010001
	 */
	if (*der != 0) /* "ignore bits", should be 0 */
		xfunc_die();
	der++;
	der = enter_der_item(der, &end); /* enter SEQ */
	/* memset(tls->server_rsa_pub_key, 0, sizeof(tls->server_rsa_pub_key)); - already is */
	der_binary_to_pstm(&tls->server_rsa_pub_key.N, der, end); /* modulus */
	der = skip_der_item(der, end);
	der_binary_to_pstm(&tls->server_rsa_pub_key.e, der, end); /* exponent */
	tls->server_rsa_pub_key.size = pstm_unsigned_bin_size(&tls->server_rsa_pub_key.N);
	dbg("server_rsa_pub_key.size:%d\n", tls->server_rsa_pub_key.size);
}

/*
 * TLS Handshake routines
 */
static int xread_tls_handshake_block(tls_state_t *tls, int min_len)
{
	struct record_hdr *xhdr;
	int len = tls_xread_record(tls);

	xhdr = (void*)tls->inbuf;
	if (len < min_len
	 || xhdr->type != RECORD_TYPE_HANDSHAKE
	 || xhdr->proto_maj != TLS_MAJ
	 || xhdr->proto_min != TLS_MIN
	) {
		tls_error_die(tls);
	}
	dbg("got HANDSHAKE\n");
	return len;
}

static ALWAYS_INLINE void fill_handshake_record_hdr(void *buf, unsigned type, unsigned len)
{
	struct handshake_hdr {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
	} *h = buf;

	len -= 4;
	h->type = type;
	h->len24_hi  = len >> 16;
	h->len24_mid = len >> 8;
	h->len24_lo  = len & 0xff;
}

//TODO: implement RFC 5746 (Renegotiation Indication Extension) - some servers will refuse to work with us otherwise
static void send_client_hello(tls_state_t *tls)
{
	struct client_hello {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t proto_maj, proto_min;
		uint8_t rand32[32];
		uint8_t session_id_len;
		/* uint8_t session_id[]; */
		uint8_t cipherid_len16_hi, cipherid_len16_lo;
		uint8_t cipherid[2 * 1]; /* actually variable */
		uint8_t comprtypes_len;
		uint8_t comprtypes[1]; /* actually variable */
	};
	struct client_hello *record = tls_get_outbuf(tls, sizeof(*record));

	fill_handshake_record_hdr(record, HANDSHAKE_CLIENT_HELLO, sizeof(*record));
	record->proto_maj = TLS_MAJ;	/* the "requested" version of the protocol, */
	record->proto_min = TLS_MIN;	/* can be higher than one in record headers */
	tls_get_random(record->rand32, sizeof(record->rand32));
	if (TLS_DEBUG_FIXED_SECRETS)
		memset(record->rand32, 0x11, sizeof(record->rand32));
	memcpy(tls->client_and_server_rand32, record->rand32, sizeof(record->rand32));
	record->session_id_len = 0;
	record->cipherid_len16_hi = 0;
	record->cipherid_len16_lo = 2 * 1;
	record->cipherid[0] = CIPHER_ID >> 8;
	record->cipherid[1] = CIPHER_ID & 0xff;
	record->comprtypes_len = 1;
	record->comprtypes[0] = 0;

//TODO: send options, at least SNI.

	dbg(">> CLIENT_HELLO\n");
	xwrite_and_update_handshake_hash(tls, sizeof(*record));
}

static void get_server_hello(tls_state_t *tls)
{
	struct server_hello {
		struct record_hdr xhdr;
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t proto_maj, proto_min;
		uint8_t rand32[32]; /* first 4 bytes are unix time in BE format */
		uint8_t session_id_len;
		uint8_t session_id[32];
		uint8_t cipherid_hi, cipherid_lo;
		uint8_t comprtype;
		/* extensions may follow, but only those which client offered in its Hello */
	};
	struct server_hello *hp;
	uint8_t *cipherid;

	xread_tls_handshake_block(tls, 74);

	hp = (void*)tls->inbuf;
	// 74 bytes:
	// 02  000046 03|03   58|78|cf|c1 50|a5|49|ee|7e|29|48|71|fe|97|fa|e8|2d|19|87|72|90|84|9d|37|a3|f0|cb|6f|5f|e3|3c|2f |20  |d8|1a|78|96|52|d6|91|01|24|b3|d6|5b|b7|d0|6c|b3|e1|78|4e|3c|95|de|74|a0|ba|eb|a7|3a|ff|bd|a2|bf |00|9c |00|
	//SvHl len=70 maj.min unixtime^^^ 28randbytes^^^^^^^^^^^^^^^^^^^^^^^^^^^^_^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^_^^^ slen sid32bytes^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ cipSel comprSel
	if (hp->type != HANDSHAKE_SERVER_HELLO
	 || hp->len24_hi  != 0
	 || hp->len24_mid != 0
	 /* hp->len24_lo checked later */
	 || hp->proto_maj != TLS_MAJ
	 || hp->proto_min != TLS_MIN
	) {
		tls_error_die(tls);
	}

	cipherid = &hp->cipherid_hi;
	if (hp->session_id_len != 32) {
		if (hp->session_id_len != 0)
			tls_error_die(tls);

		// session_id_len == 0: no session id
		// "The server
		// may return an empty session_id to indicate that the session will
		// not be cached and therefore cannot be resumed."
		cipherid -= 32;
		hp->len24_lo += 32; /* what len would be if session id would be present */
	}

	if (hp->len24_lo < 70
	 || cipherid[0]  != (CIPHER_ID >> 8)
	 || cipherid[1]  != (CIPHER_ID & 0xff)
	 || cipherid[2]  != 0 /* comprtype */
	) {
		tls_error_die(tls);
	}

	dbg("<< SERVER_HELLO\n");
	memcpy(tls->client_and_server_rand32 + 32, hp->rand32, sizeof(hp->rand32));
}

static void get_server_cert(tls_state_t *tls)
{
	struct record_hdr *xhdr;
	uint8_t *certbuf;
	int len, len1;

	len = xread_tls_handshake_block(tls, 10);

	xhdr = (void*)tls->inbuf;
	certbuf = (void*)(xhdr + 1);
	if (certbuf[0] != HANDSHAKE_CERTIFICATE)
		tls_error_die(tls);
	dbg("<< CERTIFICATE\n");
	// 4392 bytes:
	// 0b  00|11|24 00|11|21 00|05|b0 30|82|05|ac|30|82|04|94|a0|03|02|01|02|02|11|00|9f|85|bf|66|4b|0c|dd|af|ca|50|86|79|50|1b|2b|e4|30|0d...
	//Cert len=4388 ChainLen CertLen^ DER encoded X509 starts here. openssl x509 -in FILE -inform DER -noout -text
	len1 = get24be(certbuf + 1);
	if (len1 > len - 4) tls_error_die(tls);
	len = len1;
	len1 = get24be(certbuf + 4);
	if (len1 > len - 3) tls_error_die(tls);
	len = len1;
	len1 = get24be(certbuf + 7);
	if (len1 > len - 3) tls_error_die(tls);
	len = len1;

	if (len)
		find_key_in_der_cert(tls, certbuf + 10, len);
}

static void send_client_key_exchange(tls_state_t *tls)
{
	struct client_key_exchange {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		/* keylen16 exists for RSA (in TLS, not in SSL), but not for some other key types */
		uint8_t keylen16_hi, keylen16_lo;
		uint8_t key[4 * 1024]; // size??
	};
//FIXME: better size estimate
	struct client_key_exchange *record = tls_get_outbuf(tls, sizeof(*record));
	uint8_t rsa_premaster[RSA_PREMASTER_SIZE];
	int len;

	tls_get_random(rsa_premaster, sizeof(rsa_premaster));
	if (TLS_DEBUG_FIXED_SECRETS)
		memset(rsa_premaster, 0x44, sizeof(rsa_premaster));
	// RFC 5246
	// "Note: The version number in the PreMasterSecret is the version
	// offered by the client in the ClientHello.client_version, not the
	// version negotiated for the connection."
	rsa_premaster[0] = TLS_MAJ;
	rsa_premaster[1] = TLS_MIN;
	len = psRsaEncryptPub(/*pool:*/ NULL,
		/* psRsaKey_t* */ &tls->server_rsa_pub_key,
		rsa_premaster, /*inlen:*/ sizeof(rsa_premaster),
		record->key, sizeof(record->key),
		data_param_ignored
	);
	record->keylen16_hi = len >> 8;
	record->keylen16_lo = len & 0xff;
	len += 2;
	record->type = HANDSHAKE_CLIENT_KEY_EXCHANGE;
	record->len24_hi  = 0;
	record->len24_mid = len >> 8;
	record->len24_lo  = len & 0xff;
	len += 4;

	dbg(">> CLIENT_KEY_EXCHANGE\n");
	xwrite_and_update_handshake_hash(tls, len);

	// RFC 5246
	// For all key exchange methods, the same algorithm is used to convert
	// the pre_master_secret into the master_secret.  The pre_master_secret
	// should be deleted from memory once the master_secret has been
	// computed.
	//      master_secret = PRF(pre_master_secret, "master secret",
	//                          ClientHello.random + ServerHello.random)
	//                          [0..47];
	// The master secret is always exactly 48 bytes in length.  The length
	// of the premaster secret will vary depending on key exchange method.
	prf_hmac_sha256(
		tls->master_secret, sizeof(tls->master_secret),
		rsa_premaster, sizeof(rsa_premaster),
		"master secret",
		tls->client_and_server_rand32, sizeof(tls->client_and_server_rand32)
	);
	dump_hex("master secret:%s\n", tls->master_secret, sizeof(tls->master_secret));

	// RFC 5246
	// 6.3.  Key Calculation
	//
	// The Record Protocol requires an algorithm to generate keys required
	// by the current connection state (see Appendix A.6) from the security
	// parameters provided by the handshake protocol.
	//
	// The master secret is expanded into a sequence of secure bytes, which
	// is then split to a client write MAC key, a server write MAC key, a
	// client write encryption key, and a server write encryption key.  Each
	// of these is generated from the byte sequence in that order.  Unused
	// values are empty.  Some AEAD ciphers may additionally require a
	// client write IV and a server write IV (see Section 6.2.3.3).
	//
	// When keys and MAC keys are generated, the master secret is used as an
	// entropy source.
	//
	// To generate the key material, compute
	//
	//    key_block = PRF(SecurityParameters.master_secret,
	//                    "key expansion",
	//                    SecurityParameters.server_random +
	//                    SecurityParameters.client_random);
	//
	// until enough output has been generated.  Then, the key_block is
	// partitioned as follows:
	//
	//    client_write_MAC_key[SecurityParameters.mac_key_length]
	//    server_write_MAC_key[SecurityParameters.mac_key_length]
	//    client_write_key[SecurityParameters.enc_key_length]
	//    server_write_key[SecurityParameters.enc_key_length]
	//    client_write_IV[SecurityParameters.fixed_iv_length]
	//    server_write_IV[SecurityParameters.fixed_iv_length]
	{
		uint8_t tmp64[64];

		/* make "server_rand32 + client_rand32" */
		memcpy(&tmp64[0] , &tls->client_and_server_rand32[32], 32);
		memcpy(&tmp64[32], &tls->client_and_server_rand32[0] , 32);

		prf_hmac_sha256(
			tls->client_write_MAC_key, 2 * (SHA256_OUTSIZE + AES256_KEYSIZE),
			// also fills:
			// server_write_MAC_key[SHA256_OUTSIZE]
			// client_write_key[AES256_KEYSIZE]
			// server_write_key[AES256_KEYSIZE]
			tls->master_secret, sizeof(tls->master_secret),
			"key expansion",
			tmp64, 64
		);
		dump_hex("client_write_MAC_key:%s\n",
			tls->client_write_MAC_key, sizeof(tls->client_write_MAC_key)
		);
		dump_hex("client_write_key:%s\n",
			tls->client_write_key, sizeof(tls->client_write_key)
		);
	}
}

static const uint8_t rec_CHANGE_CIPHER_SPEC[] = {
	RECORD_TYPE_CHANGE_CIPHER_SPEC, TLS_MAJ, TLS_MIN, 00, 01,
	01
};

static void send_change_cipher_spec(tls_state_t *tls)
{
	dbg(">> CHANGE_CIPHER_SPEC\n");
	xwrite(tls->fd, rec_CHANGE_CIPHER_SPEC, sizeof(rec_CHANGE_CIPHER_SPEC));
}

// 7.4.9.  Finished
// A Finished message is always sent immediately after a change
// cipher spec message to verify that the key exchange and
// authentication processes were successful.  It is essential that a
// change cipher spec message be received between the other handshake
// messages and the Finished message.
//...
// The Finished message is the first one protected with the just
// negotiated algorithms, keys, and secrets.  Recipients of Finished
// messages MUST verify that the contents are correct.  Once a side
// has sent its Finished message and received and validated the
// Finished message from its peer, it may begin to send and receive
// application data over the connection.
//...
// struct {
//     opaque verify_data[verify_data_length];
// } Finished;
//
// verify_data
//    PRF(master_secret, finished_label, Hash(handshake_messages))
//       [0..verify_data_length-1];
//
// finished_label
//    For Finished messages sent by the client, the string
//    "client finished".  For Finished messages sent by the server,
//    the string "server finished".
//
// Hash denotes a Hash of the handshake messages.  For the PRF
// defined in Section 5, the Hash MUST be the Hash used as the basis
// for the PRF.  Any cipher suite which defines a different PRF MUST
// also define the Hash to use in the Finished computation.
//
// In previous versions of TLS, the verify_data was always 12 octets
// long.  In the current version of TLS, it depends on the cipher
// suite.  Any cipher suite which does not explicitly specify
// verify_data_length has a verify_data_length equal to 12.  This
// includes all existing cipher suites.
static void send_client_finished(tls_state_t *tls)
{
	struct finished {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t prf_result[12];
	};
	struct finished *record = tls_get_outbuf(tls, sizeof(*record));
	uint8_t handshake_hash[SHA256_OUTSIZE];

	fill_handshake_record_hdr(record, HANDSHAKE_FINISHED, sizeof(*record));

	sha256_peek(&tls->handshake_sha256_ctx, handshake_hash);
	prf_hmac_sha256(record->prf_result, sizeof(record->prf_result),
			tls->master_secret, sizeof(tls->master_secret),
			"client finished",
			handshake_hash, sizeof(handshake_hash)
	);
	dump_hex("from secret: %s\n", tls->master_secret, sizeof(tls->master_secret));
	dump_hex("from labelSeed: %s", "client finished", sizeof("client finished")-1);
	dump_hex("%s\n", handshake_hash, sizeof(handshake_hash));
	dump_hex("=> digest: %s\n", record->prf_result, sizeof(record->prf_result));

	dbg(">> FINISHED\n");
	xwrite_encrypted(tls, sizeof(*record), RECORD_TYPE_HANDSHAKE);
}

static void tls_handshake(tls_state_t *tls)
{
	// Client              RFC 5246                Server
	// (*) - optional messages, not always sent
	//
	// ClientHello          ------->
	//                                        ServerHello
	//                                       Certificate*
	//                                 ServerKeyExchange*
	//                                CertificateRequest*
	//                      <-------      ServerHelloDone
	// Certificate*
	// ClientKeyExchange
	// CertificateVerify*
	// [ChangeCipherSpec]
	// Finished             ------->
	//                                 [ChangeCipherSpec]
	//                      <-------             Finished
	// Application Data     <------>     Application Data
	int len;

	send_client_hello(tls);
	get_server_hello(tls);

	// RFC 5246
	// The server MUST send a Certificate message whenever the agreed-
	// upon key exchange method uses certificates for authentication
	// (this includes all key exchange methods defined in this document
	// except DH_anon).  This message will always immediately follow the
	// ServerHello message.
	//
	// IOW: in practice, Certificate *always* follows.
	// (for example, kernel.org does not even accept DH_anon cipher id)
	get_server_cert(tls);

	len = xread_tls_handshake_block(tls, 4);
	if (tls->inbuf[RECHDR_LEN] == HANDSHAKE_SERVER_KEY_EXCHANGE) {
		// 459 bytes:
		// 0c   00|01|c7 03|00|17|41|04|87|94|2e|2f|68|d0|c9|f4|97|a8|2d|ef|ed|67|ea|c6|f3|b3|56|47|5d|27|b6|bd|ee|70|25|30|5e|b0|8e|f6|21|5a...
		//SvKey len=455^
		// with TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA: 461 bytes:
		// 0c   00|01|c9 03|00|17|41|04|cd|9b|b4|29|1f|f6|b0|c2|84|82|7f|29|6a|47|4e|ec|87|0b|c1|9c|69|e1|f8|c6|d0|53|e9|27|90|a5|c8|02|15|75...
		dbg("<< SERVER_KEY_EXCHANGE len:%u\n", len);
//probably need to save it
		xread_tls_handshake_block(tls, 4);
	}

//	if (tls->inbuf[RECHDR_LEN] == HANDSHAKE_CERTIFICATE_REQUEST) {
//		dbg("<< CERTIFICATE_REQUEST\n");
// RFC 5246: (in response to this,) "If no suitable certificate is available,
// the client MUST send a certificate message containing no
// certificates.  That is, the certificate_list structure has a
// length of zero. ...
// Client certificates are sent using the Certificate structure
// defined in Section 7.4.2."
// (i.e. the same format as server certs)
//		xread_tls_handshake_block(tls, 4);
//	}

	if (tls->inbuf[RECHDR_LEN] != HANDSHAKE_SERVER_HELLO_DONE)
		tls_error_die(tls);
	// 0e 000000 (len:0)
	dbg("<< SERVER_HELLO_DONE\n");

	send_client_key_exchange(tls);

	send_change_cipher_spec(tls);
	/* from now on we should send encrypted */
	/* tls->write_seq64_be = 0; - already is */
	tls->encrypt_on_write = 1;

	send_client_finished(tls);

	/* Get CHANGE_CIPHER_SPEC */
	len = tls_xread_record(tls);
	if (len != 1 || memcmp(tls->inbuf, rec_CHANGE_CIPHER_SPEC, 6) != 0)
		tls_error_die(tls);
	dbg("<< CHANGE_CIPHER_SPEC\n");
	if (CIPHER_ID == TLS_RSA_WITH_NULL_SHA256)
		tls->min_encrypted_len_on_read = SHA256_OUTSIZE;
	else
		/* all incoming packets now should be encrypted and have IV + MAC + padding */
		tls->min_encrypted_len_on_read = AES_BLOCKSIZE + SHA256_OUTSIZE + AES_BLOCKSIZE;

	/* Get (encrypted) FINISHED from the server */
	len = tls_xread_record(tls);
	if (len < 4 || tls->inbuf[RECHDR_LEN] != HANDSHAKE_FINISHED)
		tls_error_die(tls);
	dbg("<< FINISHED\n");
	/* application data can be sent/received */
}

static void tls_xwrite(tls_state_t *tls, int len)
{
	dbg(">> DATA\n");
	xwrite_encrypted(tls, len, RECORD_TYPE_APPLICATION_DATA);
}

// To run a test server using openssl:
// openssl req -x509 -newkey rsa:$((4096/4*3)) -keyout key.pem -out server.pem -nodes -days 99999 -subj '/CN=localhost'
// openssl s_server -key key.pem -cert server.pem -debug -tls1_2 -no_tls1 -no_tls1_1
//
// Unencryped SHA256 example:
// openssl req -x509 -newkey rsa:$((4096/4*3)) -keyout key.pem -out server.pem -nodes -days 99999 -subj '/CN=localhost'
// openssl s_server -key key.pem -cert server.pem -debug -tls1_2 -no_tls1 -no_tls1_1 -cipher NULL
// openssl s_client -connect 127.0.0.1:4433 -debug -tls1_2 -no_tls1 -no_tls1_1 -cipher NULL-SHA256
//
// Talk to kernel.org:
// printf "GET / HTTP/1.1\r\nHost: kernel.org\r\n\r\n" | ./busybox tls kernel.org

int tls_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tls_main(int argc UNUSED_PARAM, char **argv)
{
	tls_state_t *tls;
	fd_set readfds;
	int inbuf_size;
	const int INBUF_STEP = 4 * 1024;
	int cfd;


	// INIT_G();
	// getopt32(argv, "myopts")

	if (!argv[1])
		bb_show_usage();

	cfd = create_and_connect_stream_or_die(argv[1], 443);

	tls = new_tls_state();
	tls->fd = cfd;
	tls_handshake(tls);

	/* Select loop copying stdin to cfd, and cfd to stdout */
	FD_ZERO(&readfds);
	FD_SET(cfd, &readfds);
	FD_SET(STDIN_FILENO, &readfds);

	inbuf_size = INBUF_STEP;
	for (;;) {
		fd_set testfds;
		int nread;

		testfds = readfds;
		if (select(cfd + 1, &testfds, NULL, NULL, NULL) < 0)
			bb_perror_msg_and_die("select");

		if (FD_ISSET(STDIN_FILENO, &testfds)) {
			void *buf;

			dbg("STDIN HAS DATA\n");
			buf = tls_get_outbuf(tls, inbuf_size);
			nread = safe_read(STDIN_FILENO, buf, inbuf_size);
			if (nread < 1) {
				/* We'd want to do this: */
				/* Close outgoing half-connection so they get EOF,
				 * but leave incoming alone so we can see response
				 */
				//shutdown(cfd, SHUT_WR);
				/* But TLS has no way to encode this,
				 * doubt it's ok to do it "raw"
				 */
				FD_CLR(STDIN_FILENO, &readfds);
				tls_free_outbuf(tls);
			} else {
				if (nread == inbuf_size) {
					/* TLS has per record overhead, if input comes fast,
					 * read, encrypt and send bigger chunks
					 */
					inbuf_size += INBUF_STEP;
					if (inbuf_size > MAX_OUTBUF)
						inbuf_size = MAX_OUTBUF;
				}
				tls_xwrite(tls, nread);
			}
		}
		if (FD_ISSET(cfd, &testfds)) {
			dbg("NETWORK HAS DATA\n");
 read_record:
			nread = tls_xread_record(tls);
			if (nread < 1) {
				/* TLS protocol has no real concept of one-sided shutdowns:
				 * if we get "TLS EOF" from the peer, writes will fail too
				 */
				//FD_CLR(cfd, &readfds);
				//close(STDOUT_FILENO);
				//continue;
				break;
			}
			if (tls->inbuf[0] != RECORD_TYPE_APPLICATION_DATA)
				bb_error_msg_and_die("unexpected record type %d", tls->inbuf[0]);
			xwrite(STDOUT_FILENO, tls->inbuf + RECHDR_LEN, nread);
			/* We may already have a complete next record buffered,
			 * can process it without network reads (and possible blocking)
			 */
			if (tls_has_buffered_record(tls))
				goto read_record;
		}
	}

	return EXIT_SUCCESS;
}
