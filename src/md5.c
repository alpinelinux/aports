/* md5.c - Compute MD5 checksum of files or strings according to the
 *         definition of MD5 in RFC 1321 from April 1992.
 * Copyright (C) 1995-1999 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Written by Ulrich Drepper <drepper@gnu.ai.mit.edu> */
/* Hacked to work with BusyBox by Alfred M. Szmidt <ams@trillian.itslinux.org> */

/* Sucked directly into ipkg since the md5sum functions aren't in libbb
   Dropped a few functions since ipkg only needs md5_stream.
   Got rid of evil, twisted defines of FALSE=1 and TRUE=0
   6 March 2002 Carl Worth <cworth@east.isi.edu>
*/

/*
 * June 29, 2001        Manuel Novoa III
 *
 * Added MD5SUM_SIZE_VS_SPEED configuration option.
 *
 * Current valid values, with data from my system for comparison, are:
 *   (using uClibc and running on linux-2.4.4.tar.bz2)
 *                     user times (sec)  text size (386)
 *     0 (fastest)         1.1                6144
 *     1                   1.4                5392
 *     2                   3.0                5088
 *     3 (smallest)        5.1                4912
 */

#define MD5SUM_SIZE_VS_SPEED 0

/**********************************************************************/

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <sys/types.h>
#if defined HAVE_LIMITS_H
# include <limits.h>
#endif

#include "md5.h"

/* Handle endian-ness */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SWAP(n) (n)
#else
#define SWAP(n) ((n << 24) | ((n&65280)<<8) | ((n&16711680)>>8) | (n>>24))
#endif

#if MD5SUM_SIZE_VS_SPEED == 0
/* This array contains the bytes used to pad the buffer to the next
   64-byte boundary.  (RFC 1321, 3.1: Step 1)  */
static const unsigned char fillbuf[64] = { 0x80, 0 /* , 0, 0, ...  */  };
#endif

/* These are the four functions used in the four steps of the MD5 algorithm
   and defined in the RFC 1321.  The first function is a little bit optimized
   (as found in Colin Plumbs public domain implementation).  */
/* #define FF(b, c, d) ((b & c) | (~b & d)) */
#define FF(b, c, d) (d ^ (b & (c ^ d)))
#define FG(b, c, d) FF (d, b, c)
#define FH(b, c, d) (b ^ c ^ d)
#define FI(b, c, d) (c ^ (b | ~d))

#define CYCLIC(w, s) (w = (w << s) | (w >> (32 - s)))

/* Process LEN bytes of BUFFER, accumulating context into CTX.
   It is assumed that LEN % 64 == 0.  */
static void md5_process_block(struct md5_ctx *ctx,
			      const void *buffer, size_t len)
{
	md5_uint32 correct_words[16];
	const md5_uint32 *words = buffer;
	size_t nwords = len / sizeof(md5_uint32);
	const md5_uint32 *endp = words + nwords;
#if MD5SUM_SIZE_VS_SPEED > 0
	static const md5_uint32 C_array[] = {
		/* round 1 */
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
		0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
		0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		/* round 2 */
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
		0xd62f105d, 0x2441453,  0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
		0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		/* round 3 */
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
		0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x4881d05,
		0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		/* round 4 */
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
		0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
		0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
	};

	static const char P_array[] = {
#if MD5SUM_SIZE_VS_SPEED > 1
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, /* 1 */
#endif
		1, 6, 11, 0, 5, 10, 15, 4, 9, 14, 3, 8, 13, 2, 7, 12, /* 2 */
		5, 8, 11, 14, 1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2, /* 3 */
		0, 7, 14, 5, 12, 3, 10, 1, 8, 15, 6, 13, 4, 11, 2, 9  /* 4 */
	};

#if MD5SUM_SIZE_VS_SPEED > 1
	static const char S_array[] = {
		7, 12, 17, 22,
		5, 9, 14, 20,
		4, 11, 16, 23,
		6, 10, 15, 21
	};
#endif
#endif

	md5_uint32 A = ctx->A;
	md5_uint32 B = ctx->B;
	md5_uint32 C = ctx->C;
	md5_uint32 D = ctx->D;

	/* First increment the byte count.  RFC 1321 specifies the possible
	   length of the file up to 2^64 bits.  Here we only compute the
	   number of bytes.  Do a double word increment.  */
	ctx->total[0] += len;
	if (ctx->total[0] < len)
		++ctx->total[1];

	/* Process all bytes in the buffer with 64 bytes in each round of
	   the loop.  */
	while (words < endp) {
		md5_uint32 *cwp = correct_words;
		md5_uint32 A_save = A;
		md5_uint32 B_save = B;
		md5_uint32 C_save = C;
		md5_uint32 D_save = D;

#if MD5SUM_SIZE_VS_SPEED > 1
		const md5_uint32 *pc;
		const char *pp;
		const char *ps;
		int i;
		md5_uint32 temp;

		for ( i=0 ; i < 16 ; i++ ) {
			cwp[i] = SWAP(words[i]);
		}
		words += 16;

#if MD5SUM_SIZE_VS_SPEED > 2
		pc = C_array; pp = P_array; ps = S_array - 4;

		for ( i = 0 ; i < 64 ; i++ ) {
			if ((i&0x0f) == 0) ps += 4;
			temp = A;
			switch (i>>4) {
			case 0:
				temp += FF(B,C,D);
				break;
			case 1:
				temp += FG(B,C,D);
				break;
			case 2:
				temp += FH(B,C,D);
				break;
			case 3:
				temp += FI(B,C,D);
			}
			temp += cwp[(int)(*pp++)] + *pc++;
			temp = CYCLIC(temp, ps[i&3]);
			temp += B;
			A = D; D = C; C = B; B = temp;
		}
#else
		pc = C_array; pp = P_array; ps = S_array;

		for ( i = 0 ; i < 16 ; i++ ) {
			temp = A + FF(B,C,D) + cwp[(int)(*pp++)] + *pc++;
			temp = CYCLIC (temp, ps[i&3]);
			temp += B;
			A = D; D = C; C = B; B = temp;
		}

		ps += 4;
		for ( i = 0 ; i < 16 ; i++ ) {
			temp = A + FG(B,C,D) + cwp[(int)(*pp++)] + *pc++;
			temp = CYCLIC (temp, ps[i&3]);
			temp += B;
			A = D; D = C; C = B; B = temp;
		}
		ps += 4;
		for ( i = 0 ; i < 16 ; i++ ) {
			temp = A + FH(B,C,D) + cwp[(int)(*pp++)] + *pc++;
			temp = CYCLIC (temp, ps[i&3]);
			temp += B;
			A = D; D = C; C = B; B = temp;
		}
		ps += 4;
		for ( i = 0 ; i < 16 ; i++ ) {
			temp = A + FI(B,C,D) + cwp[(int)(*pp++)] + *pc++;
			temp = CYCLIC (temp, ps[i&3]);
			temp += B;
			A = D; D = C; C = B; B = temp;
		}

#endif
#else
		/* First round: using the given function, the context and a constant
		   the next context is computed.  Because the algorithms processing
		   unit is a 32-bit word and it is determined to work on words in
		   little endian byte order we perhaps have to change the byte order
		   before the computation.  To reduce the work for the next steps
		   we store the swapped words in the array CORRECT_WORDS.  */

#define OP(a, b, c, d, s, T)						\
	do								\
	{								\
	a += FF (b, c, d) + (*cwp++ = SWAP (*words)) + T;		\
	++words;							\
	CYCLIC (a, s);						\
	a += b;							\
	}								\
	while (0)

		/* Before we start, one word to the strange constants.
		 They are defined in RFC 1321 as

		 T[i] = (int) (4294967296.0 * fabs (sin (i))), i=1..64
		 */

#if MD5SUM_SIZE_VS_SPEED == 1
		const md5_uint32 *pc;
		const char *pp;
		int i;
#endif

		/* Round 1.  */
#if MD5SUM_SIZE_VS_SPEED == 1
		pc = C_array;
		for ( i=0 ; i < 4 ; i++ ) {
			OP(A, B, C, D, 7, *pc++);
			OP(D, A, B, C, 12, *pc++);
			OP(C, D, A, B, 17, *pc++);
			OP(B, C, D, A, 22, *pc++);
		}
#else
		OP(A, B, C, D, 7, 0xd76aa478);
		OP(D, A, B, C, 12, 0xe8c7b756);
		OP(C, D, A, B, 17, 0x242070db);
		OP(B, C, D, A, 22, 0xc1bdceee);
		OP(A, B, C, D, 7, 0xf57c0faf);
		OP(D, A, B, C, 12, 0x4787c62a);
		OP(C, D, A, B, 17, 0xa8304613);
		OP(B, C, D, A, 22, 0xfd469501);
		OP(A, B, C, D, 7, 0x698098d8);
		OP(D, A, B, C, 12, 0x8b44f7af);
		OP(C, D, A, B, 17, 0xffff5bb1);
		OP(B, C, D, A, 22, 0x895cd7be);
		OP(A, B, C, D, 7, 0x6b901122);
		OP(D, A, B, C, 12, 0xfd987193);
		OP(C, D, A, B, 17, 0xa679438e);
		OP(B, C, D, A, 22, 0x49b40821);
#endif

		/* For the second to fourth round we have the possibly swapped words
		   in CORRECT_WORDS.  Redefine the macro to take an additional first
		   argument specifying the function to use.  */
#undef OP
#define OP(f, a, b, c, d, k, s, T)					\
	do 								\
	{								\
	a += f (b, c, d) + correct_words[k] + T;			\
	CYCLIC (a, s);						\
	a += b;							\
	}								\
	while (0)

		/* Round 2.  */
#if MD5SUM_SIZE_VS_SPEED == 1
		pp = P_array;
		for ( i=0 ; i < 4 ; i++ ) {
			OP(FG, A, B, C, D, (int)(*pp++), 5, *pc++);
			OP(FG, D, A, B, C, (int)(*pp++), 9, *pc++);
			OP(FG, C, D, A, B, (int)(*pp++), 14, *pc++);
			OP(FG, B, C, D, A, (int)(*pp++), 20, *pc++);
		}
#else
		OP(FG, A, B, C, D, 1, 5, 0xf61e2562);
		OP(FG, D, A, B, C, 6, 9, 0xc040b340);
		OP(FG, C, D, A, B, 11, 14, 0x265e5a51);
		OP(FG, B, C, D, A, 0, 20, 0xe9b6c7aa);
		OP(FG, A, B, C, D, 5, 5, 0xd62f105d);
		OP(FG, D, A, B, C, 10, 9, 0x02441453);
		OP(FG, C, D, A, B, 15, 14, 0xd8a1e681);
		OP(FG, B, C, D, A, 4, 20, 0xe7d3fbc8);
		OP(FG, A, B, C, D, 9, 5, 0x21e1cde6);
		OP(FG, D, A, B, C, 14, 9, 0xc33707d6);
		OP(FG, C, D, A, B, 3, 14, 0xf4d50d87);
		OP(FG, B, C, D, A, 8, 20, 0x455a14ed);
		OP(FG, A, B, C, D, 13, 5, 0xa9e3e905);
		OP(FG, D, A, B, C, 2, 9, 0xfcefa3f8);
		OP(FG, C, D, A, B, 7, 14, 0x676f02d9);
		OP(FG, B, C, D, A, 12, 20, 0x8d2a4c8a);
#endif

		/* Round 3.  */
#if MD5SUM_SIZE_VS_SPEED == 1
		for ( i=0 ; i < 4 ; i++ ) {
			OP(FH, A, B, C, D, (int)(*pp++), 4, *pc++);
			OP(FH, D, A, B, C, (int)(*pp++), 11, *pc++);
			OP(FH, C, D, A, B, (int)(*pp++), 16, *pc++);
			OP(FH, B, C, D, A, (int)(*pp++), 23, *pc++);
		}
#else
		OP(FH, A, B, C, D, 5, 4, 0xfffa3942);
		OP(FH, D, A, B, C, 8, 11, 0x8771f681);
		OP(FH, C, D, A, B, 11, 16, 0x6d9d6122);
		OP(FH, B, C, D, A, 14, 23, 0xfde5380c);
		OP(FH, A, B, C, D, 1, 4, 0xa4beea44);
		OP(FH, D, A, B, C, 4, 11, 0x4bdecfa9);
		OP(FH, C, D, A, B, 7, 16, 0xf6bb4b60);
		OP(FH, B, C, D, A, 10, 23, 0xbebfbc70);
		OP(FH, A, B, C, D, 13, 4, 0x289b7ec6);
		OP(FH, D, A, B, C, 0, 11, 0xeaa127fa);
		OP(FH, C, D, A, B, 3, 16, 0xd4ef3085);
		OP(FH, B, C, D, A, 6, 23, 0x04881d05);
		OP(FH, A, B, C, D, 9, 4, 0xd9d4d039);
		OP(FH, D, A, B, C, 12, 11, 0xe6db99e5);
		OP(FH, C, D, A, B, 15, 16, 0x1fa27cf8);
		OP(FH, B, C, D, A, 2, 23, 0xc4ac5665);
#endif

		/* Round 4.  */
#if MD5SUM_SIZE_VS_SPEED == 1
		for ( i=0 ; i < 4 ; i++ ) {
			OP(FI, A, B, C, D, (int)(*pp++), 6, *pc++);
			OP(FI, D, A, B, C, (int)(*pp++), 10, *pc++);
			OP(FI, C, D, A, B, (int)(*pp++), 15, *pc++);
			OP(FI, B, C, D, A, (int)(*pp++), 21, *pc++);
		}
#else
		OP(FI, A, B, C, D, 0, 6, 0xf4292244);
		OP(FI, D, A, B, C, 7, 10, 0x432aff97);
		OP(FI, C, D, A, B, 14, 15, 0xab9423a7);
		OP(FI, B, C, D, A, 5, 21, 0xfc93a039);
		OP(FI, A, B, C, D, 12, 6, 0x655b59c3);
		OP(FI, D, A, B, C, 3, 10, 0x8f0ccc92);
		OP(FI, C, D, A, B, 10, 15, 0xffeff47d);
		OP(FI, B, C, D, A, 1, 21, 0x85845dd1);
		OP(FI, A, B, C, D, 8, 6, 0x6fa87e4f);
		OP(FI, D, A, B, C, 15, 10, 0xfe2ce6e0);
		OP(FI, C, D, A, B, 6, 15, 0xa3014314);
		OP(FI, B, C, D, A, 13, 21, 0x4e0811a1);
		OP(FI, A, B, C, D, 4, 6, 0xf7537e82);
		OP(FI, D, A, B, C, 11, 10, 0xbd3af235);
		OP(FI, C, D, A, B, 2, 15, 0x2ad7d2bb);
		OP(FI, B, C, D, A, 9, 21, 0xeb86d391);
#endif
#endif

		/* Add the starting values of the context.  */
		A += A_save;
		B += B_save;
		C += C_save;
		D += D_save;
	}

	/* Put checksum in context given as argument.  */
	ctx->A = A;
	ctx->B = B;
	ctx->C = C;
	ctx->D = D;
}

/* Initialize structure containing state of computation.
   (RFC 1321, 3.3: Step 3)  */
void md5_init(struct md5_ctx *ctx)
{
	ctx->A = 0x67452301;
	ctx->B = 0xefcdab89;
	ctx->C = 0x98badcfe;
	ctx->D = 0x10325476;

	ctx->total[0] = ctx->total[1] = 0;
	ctx->buflen = 0;
}

void md5_process(struct md5_ctx *ctx, const void *buffer, size_t len)
{
	/* When we already have some bits in our internal buffer concatenate
	   both inputs first.  */
	if (ctx->buflen != 0) {
		size_t left_over = ctx->buflen;
		size_t add = 128 - left_over > len ? len : 128 - left_over;

		memcpy(&ctx->buffer[left_over], buffer, add);
		ctx->buflen += add;

		if (left_over + add > 64) {
			md5_process_block(ctx, ctx->buffer, (left_over + add) & ~63);
			/* The regions in the following copy operation cannot overlap.  */
			memcpy(ctx->buffer, &ctx->buffer[(left_over + add) & ~63],
			       (left_over + add) & 63);
			ctx->buflen = (left_over + add) & 63;
		}

		buffer = (const char *) buffer + add;
		len -= add;
	}

	/* Process available complete blocks.  */
	if (len > 64) {
		md5_process_block(ctx, buffer, len & ~63);
		buffer = (const char *) buffer + (len & ~63);
		len &= 63;
	}

	/* Move remaining bytes in internal buffer.  */
	if (len > 0) {
		memcpy(ctx->buffer, buffer, len);
		ctx->buflen = len;
	}
}

/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
void md5_finish(struct md5_ctx *ctx, md5sum_t resbuf)
{
	/* Take yet unprocessed bytes into account.  */
	md5_uint32 bytes = ctx->buflen;
	size_t pad;

	/* Now count remaining bytes.  */
	ctx->total[0] += bytes;
	if (ctx->total[0] < bytes)
		++ctx->total[1];

	pad = bytes >= 56 ? 64 + 56 - bytes : 56 - bytes;
#if MD5SUM_SIZE_VS_SPEED > 0
	memset(&ctx->buffer[bytes], 0, pad);
	ctx->buffer[bytes] = 0x80;
#else
	memcpy(&ctx->buffer[bytes], fillbuf, pad);
#endif

	/* Put the 64-bit file length in *bits* at the end of the buffer.  */
	*(md5_uint32 *) & ctx->buffer[bytes + pad] = SWAP(ctx->total[0] << 3);
	*(md5_uint32 *) & ctx->buffer[bytes + pad + 4] =
		SWAP( ((ctx->total[1] << 3) | (ctx->total[0] >> 29)) );

	/* Process last bytes.  */
	md5_process_block(ctx, ctx->buffer, bytes + pad + 8);

	/* Put result from CTX in first 16 bytes following RESBUF.  The result is
	   always in little endian byte order, so that a byte-wise output yields
	   to the wanted ASCII representation of the message digest.

	   IMPORTANT: On some systems it is required that RESBUF is correctly
	   aligned for a 32 bits value.  */
	((md5_uint32 *) resbuf)[0] = SWAP(ctx->A);
	((md5_uint32 *) resbuf)[1] = SWAP(ctx->B);
	((md5_uint32 *) resbuf)[2] = SWAP(ctx->C);
	((md5_uint32 *) resbuf)[3] = SWAP(ctx->D);
}

