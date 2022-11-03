/*******************************************************************************
 * Copyright (c) 2018, 2019 Wind River Systems, Inc. All Rights Reserved.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    https://www.eclipse.org/legal/epl-2.0/
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Keith Holman - initial implementation and documentation
 *******************************************************************************/

#include "Base64.h"

b64_size_t Base64_decode( b64_data_t *out, b64_size_t out_len, const char *in, b64_size_t in_len )
{
#define NV 64
	static const unsigned char BASE64_DECODE_TABLE[] =
	{
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, /*  0-15 */
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, /* 16-31 */
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, 62, NV, NV, NV, 63, /* 32-47 */
		52, 53, 54, 55, 56, 57, 58, 59, 60, 61, NV, NV, NV, NV, NV, NV, /* 48-63 */
		NV,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, /* 64-79 */
		15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, NV, NV, NV, NV, NV, /* 80-95 */
		NV, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 96-111 */
		41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, NV, NV, NV, NV, NV, /* 112-127 */
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, /* 128-143 */
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, /* 144-159 */
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, /* 160-175 */
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, /* 176-191 */
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, /* 192-207 */
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, /* 208-223 */
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, /* 224-239 */
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV  /* 240-255 */
	};

	b64_size_t ret = 0u;
	b64_size_t out_count = 0u;

	/* in valid base64, length must be multiple of 4's: 0, 4, 8, 12, etc */
	while ( in_len > 3u && out_count < out_len )
	{
		int i;
		unsigned char c[4];
		for ( i = 0; i < 4; ++i, ++in )
			c[i] = BASE64_DECODE_TABLE[(int)(*in)];
		in_len -= 4u;

		/* first byte */
		*out = c[0] << 2;
		*out |= (c[1] & ~0xF) >> 4;
		++out;
		++out_count;

		if ( out_count < out_len )
		{
			/* second byte */
			*out = (c[1] & 0xF) << 4;
			if ( c[2] < NV )
			{
				*out |= (c[2] & ~0x3) >> 2;
				++out;
				++out_count;

				if ( out_count < out_len )
				{
					/* third byte */
					*out = (c[2] & 0x3) << 6;
					if ( c[3] < NV )
					{
						*out |= c[3];
						++out;
						++out_count;
					}
					else
						in_len = 0u;
				}
			}
			else
				in_len = 0u;
		}
	}

	if ( out_count <= out_len )
	{
		ret = out_count;
		if ( out_count < out_len )
			*out = '\0';
	}
	return ret;
}

b64_size_t Base64_encode( char *out, b64_size_t out_len, const b64_data_t *in, b64_size_t in_len )
{
	static const char BASE64_ENCODE_TABLE[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/=";
	b64_size_t ret = 0u;
	b64_size_t out_count = 0u;

	while ( in_len > 0u && out_count < out_len )
	{
		int i;
		unsigned char c[] = { 0, 0, 64, 64 }; /* index of '=' char */

		/* first character */
		i = *in;
		c[0] = (i & ~0x3) >> 2;

		/* second character */
		c[1] = (i & 0x3) << 4;
		--in_len;
		if ( in_len > 0u )
		{
			++in;
			i = *in;
			c[1] |= (i & ~0xF) >> 4;

			/* third character */
			c[2] = (i & 0xF) << 2;
			--in_len;
			if ( in_len > 0u )
			{
				++in;
				i = *in;
				c[2] |= (i & ~0x3F) >> 6;

				/* fourth character */
				c[3] = (i & 0x3F);
				--in_len;
				++in;
			}
		}

		/* encode the characters */
		out_count += 4u;
		for ( i = 0; i < 4 && out_count <= out_len; ++i, ++out )
			*out = BASE64_ENCODE_TABLE[c[i]];
	}

	if ( out_count <= out_len )
	{
		if ( out_count < out_len )
			*out = '\0';
		ret = out_count;
	}
	return ret;
}


b64_size_t Base64_decodeLength( const char *in, b64_size_t in_len )
{
	b64_size_t pad = 0u;

	if ( in && in_len > 1u )
		pad += ( in[in_len - 2u] == '=' ? 1u : 0u );
	if ( in && in_len > 0u )
		pad += ( in[in_len - 1u] == '=' ? 1u : 0u );
	return (in_len / 4u * 3u) - pad;
}

b64_size_t Base64_encodeLength( const b64_data_t *in, b64_size_t in_len )
{
	return ((4u * in_len / 3u) + 3u) & ~0x3;
}
