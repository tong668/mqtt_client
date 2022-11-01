/*******************************************************************************
 * Copyright (c) 2018 Wind River Systems, Inc. All Rights Reserved.
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

#include "SHA1.h"
#include <endian.h>
#include <string.h>

static unsigned char pad[64] = {
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int SHA1_Init(SHA_CTX *ctx) {
    int ret = 0;
    if (ctx) {
        ctx->h[0] = 0x67452301;
        ctx->h[1] = 0xEFCDAB89;
        ctx->h[2] = 0x98BADCFE;
        ctx->h[3] = 0x10325476;
        ctx->h[4] = 0xC3D2E1F0;
        ctx->size = 0u;
        ctx->total = 0u;
        ret = 1;
    }
    return ret;
}

#define ROTATE_LEFT32(a, n)  (((a) << (n)) | ((a) >> (32 - (n))))

static void SHA1_ProcessBlock(SHA_CTX *ctx) {
    uint32_t blks[5];
    uint32_t *w;
    int i;

    /* initialize */
    for (i = 0; i < 5; ++i)
        blks[i] = ctx->h[i];

    w = ctx->w;

    /* perform SHA-1 hash */
    for (i = 0; i < 16; ++i)
        w[i] = be32toh(w[i]);

    for (i = 0; i < 80; ++i) {
        int tmp;
        if (i >= 16)
            w[i & 0x0F] = ROTATE_LEFT32(w[(i + 13) & 0x0F] ^ w[(i + 8) & 0x0F] ^ w[(i + 2) & 0x0F] ^ w[i & 0x0F], 1);

        if (i < 20)
            tmp = ROTATE_LEFT32(blks[0], 5) + ((blks[1] & blks[2]) | (~(blks[1]) & blks[3])) + blks[4] + w[i & 0x0F] +
                  0x5A827999;
        else if (i < 40)
            tmp = ROTATE_LEFT32(blks[0], 5) + (blks[1] ^ blks[2] ^ blks[3]) + blks[4] + w[i & 0x0F] + 0x6ED9EBA1;
        else if (i < 60)
            tmp = ROTATE_LEFT32(blks[0], 5) + ((blks[1] & blks[2]) | (blks[1] & blks[3]) | (blks[2] & blks[3])) +
                  blks[4] + w[i & 0x0F] + 0x8F1BBCDC;
        else
            tmp = ROTATE_LEFT32(blks[0], 5) + (blks[1] ^ blks[2] ^ blks[3]) + blks[4] + w[i & 0x0F] + 0xCA62C1D6;

        /* update registers */
        blks[4] = blks[3];
        blks[3] = blks[2];
        blks[2] = ROTATE_LEFT32(blks[1], 30);
        blks[1] = blks[0];
        blks[0] = tmp;
    }

    /* update of hash */
    for (i = 0; i < 5; ++i)
        ctx->h[i] += blks[i];
}

int SHA1_Final(unsigned char *md, SHA_CTX *ctx) {
    int i;
    int ret = 0;
    size_t pad_amount;
    uint64_t total;

    /* length before pad */
    total = ctx->total * 8;

    if (ctx->size < 56)
        pad_amount = 56 - ctx->size;
    else
        pad_amount = 64 + 56 - ctx->size;

    SHA1_Update(ctx, pad, pad_amount);

    ctx->w[14] = htobe32((uint32_t) (total >> 32));
    ctx->w[15] = htobe32((uint32_t) total);

    SHA1_ProcessBlock(ctx);

    for (i = 0; i < 5; ++i)
        ctx->h[i] = htobe32(ctx->h[i]);

    if (md) {
        memcpy(md, &ctx->h[0], SHA1_DIGEST_LENGTH);
        ret = 1;
    }

    return ret;
}

int SHA1_Update(SHA_CTX *ctx, const void *data, size_t len) {
    while (len > 0) {
        unsigned int n = 64 - ctx->size;
        if (len < n)
            n = len;

        memcpy(ctx->buffer + ctx->size, data, n);

        ctx->size += n;
        ctx->total += n;

        data = (uint8_t *) data + n;
        len -= n;

        if (ctx->size == 64) {
            SHA1_ProcessBlock(ctx);
            ctx->size = 0;
        }
    }
    return 1;
}



