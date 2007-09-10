/*
 *  Copyright(C) 2006 Cameron Rich
 *
 *  This license is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This license is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this license; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * The testing of the crypto and ssl stuff goes here. Keeps the individual code
 * modules from being uncluttered with test code.
 *
 * This is test code - I make no apologies for the quality!
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef WIN32
#include <pthread.h>
#endif

#include "ssl.h"

#define DEFAULT_CERT            "../ssl/test/axTLS.x509_512.cer"
#define DEFAULT_KEY             "../ssl/test/axTLS.key_512"     
//#define DEFAULT_SVR_OPTION      SSL_DISPLAY_BYTES|SSL_DISPLAY_STATES
#define DEFAULT_SVR_OPTION      0
#define DEFAULT_CLNT_OPTION     0
//#define DEFAULT_CLNT_OPTION      SSL_DISPLAY_BYTES|SSL_DISPLAY_STATES

static int g_port = 19001;

/**************************************************************************
 * AES tests 
 * 
 * Run through a couple of the RFC3602 tests to verify that AES is correct.
 **************************************************************************/
#define TEST1_SIZE  16
#define TEST2_SIZE  32

static int AES_test(BI_CTX *bi_ctx)
{
    AES_CTX aes_key;
    int res = 1;
    uint8_t key[TEST1_SIZE];
    uint8_t iv[TEST1_SIZE];

    {
        /*
            Case #1: Encrypting 16 bytes (1 block) using AES-CBC
            Key       : 0x06a9214036b8a15b512e03d534120006
            IV        : 0x3dafba429d9eb430b422da802c9fac41
            Plaintext : "Single block msg"
            Ciphertext: 0xe353779c1079aeb82708942dbe77181a

        */
        char *in_str =  "Single block msg";
        uint8_t ct[TEST1_SIZE];
        uint8_t enc_data[TEST1_SIZE];
        uint8_t dec_data[TEST1_SIZE];

        bigint *key_bi = bi_str_import(
                bi_ctx, "06A9214036B8A15B512E03D534120006");
        bigint *iv_bi = bi_str_import(
                bi_ctx, "3DAFBA429D9EB430B422DA802C9FAC41");
        bigint *ct_bi = bi_str_import(
                bi_ctx, "E353779C1079AEB82708942DBE77181A");
        bi_export(bi_ctx, key_bi, key, TEST1_SIZE);
        bi_export(bi_ctx, iv_bi, iv, TEST1_SIZE);
        bi_export(bi_ctx, ct_bi, ct, TEST1_SIZE);

        AES_set_key(&aes_key, key, iv, AES_MODE_128);
        AES_cbc_encrypt(&aes_key, (const uint8_t *)in_str, 
                enc_data, sizeof(enc_data));
        if (memcmp(enc_data, ct, sizeof(ct)))
        {
            fprintf(stderr, "Error: AES ENCRYPT #1 failed\n");
            goto end;
        }

        AES_set_key(&aes_key, key, iv, AES_MODE_128);
        AES_convert_key(&aes_key);
        AES_cbc_decrypt(&aes_key, enc_data, dec_data, sizeof(enc_data));

        if (memcmp(dec_data, in_str, sizeof(dec_data)))
        {
            fprintf(stderr, "Error: AES DECRYPT #1 failed\n");
            goto end;
        }
    }

    {
        /*
            Case #2: Encrypting 32 bytes (2 blocks) using AES-CBC 
            Key       : 0xc286696d887c9aa0611bbb3e2025a45a
            IV        : 0x562e17996d093d28ddb3ba695a2e6f58
            Plaintext : 0x000102030405060708090a0b0c0d0e0f
                          101112131415161718191a1b1c1d1e1f
            Ciphertext: 0xd296cd94c2cccf8a3a863028b5e1dc0a
                          7586602d253cfff91b8266bea6d61ab1
        */
        uint8_t in_data[TEST2_SIZE];
        uint8_t ct[TEST2_SIZE];
        uint8_t enc_data[TEST2_SIZE];
        uint8_t dec_data[TEST2_SIZE];

        bigint *in_bi = bi_str_import(bi_ctx,
            "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
        bigint *key_bi = bi_str_import(
                bi_ctx, "C286696D887C9AA0611BBB3E2025A45A");
        bigint *iv_bi = bi_str_import(
                bi_ctx, "562E17996D093D28DDB3BA695A2E6F58");
        bigint *ct_bi = bi_str_import(bi_ctx,
            "D296CD94C2CCCF8A3A863028B5E1DC0A7586602D253CFFF91B8266BEA6D61AB1");
        bi_export(bi_ctx, in_bi, in_data, TEST2_SIZE);
        bi_export(bi_ctx, key_bi, key, TEST1_SIZE);
        bi_export(bi_ctx, iv_bi, iv, TEST1_SIZE);
        bi_export(bi_ctx, ct_bi, ct, TEST2_SIZE);

        AES_set_key(&aes_key, key, iv, AES_MODE_128);
        AES_cbc_encrypt(&aes_key, (const uint8_t *)in_data, 
                enc_data, sizeof(enc_data));

        if (memcmp(enc_data, ct, sizeof(ct)))
        {
            fprintf(stderr, "Error: ENCRYPT #2 failed\n");
            goto end;
        }

        AES_set_key(&aes_key, key, iv, AES_MODE_128);
        AES_convert_key(&aes_key);
        AES_cbc_decrypt(&aes_key, enc_data, dec_data, sizeof(enc_data));
        if (memcmp(dec_data, in_data, sizeof(dec_data)))
        {
            fprintf(stderr, "Error: DECRYPT #2 failed\n");
            goto end;
        }
    }

    res = 0;
    printf("All AES tests passed\n");

end:
    return res;
}

/**************************************************************************
 * RC4 tests 
 *
 * ARC4 tests vectors from OpenSSL (crypto/rc4/rc4test.c)
 **************************************************************************/
static const uint8_t keys[7][30]=
{
    {8,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef},
    {8,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef},
    {8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {4,0xef,0x01,0x23,0x45},
    {8,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef},
    {4,0xef,0x01,0x23,0x45},
};

static const uint8_t data_len[7]={8,8,8,20,28,10};
static uint8_t data[7][30]=
{
    {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0xff},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0xff},
        {0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
            0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
            0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
            0x12,0x34,0x56,0x78,0xff},
            {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
            {0},
};

static const uint8_t output[7][30]=
{
    {0x75,0xb7,0x87,0x80,0x99,0xe0,0xc5,0x96,0x00},
    {0x74,0x94,0xc2,0xe7,0x10,0x4b,0x08,0x79,0x00},
    {0xde,0x18,0x89,0x41,0xa3,0x37,0x5d,0x3a,0x00},
    {0xd6,0xa1,0x41,0xa7,0xec,0x3c,0x38,0xdf,
        0xbd,0x61,0x5a,0x11,0x62,0xe1,0xc7,0xba,
        0x36,0xb6,0x78,0x58,0x00},
        {0x66,0xa0,0x94,0x9f,0x8a,0xf7,0xd6,0x89,
            0x1f,0x7f,0x83,0x2b,0xa8,0x33,0xc0,0x0c,
            0x89,0x2e,0xbe,0x30,0x14,0x3c,0xe2,0x87,
            0x40,0x01,0x1e,0xcf,0x00},
            {0xd6,0xa1,0x41,0xa7,0xec,0x3c,0x38,0xdf,0xbd,0x61,0x00},
            {0},
};

static int RC4_test(BI_CTX *bi_ctx)
{
    int i, res = 1;
    RC4_CTX s;

    for (i = 0; i < 6; i++)
    {
        RC4_setup(&s, &keys[i][1], keys[i][0]);
        RC4_crypt(&s, data[i], data[i], data_len[i]);

        if (memcmp(data[i], output[i], data_len[i]))
        {
            fprintf(stderr, "Error: RC4 CRYPT #%d failed\n", i);
            goto end;
        }
    }

    res = 0;
    printf("All RC4 tests passed\n");

end:
    return res;
}

/**************************************************************************
 * SHA1 tests 
 *
 * Run through a couple of the RFC3174 tests to verify that SHA1 is correct.
 **************************************************************************/
static int SHA1_test(BI_CTX *bi_ctx)
{
    SHA1_CTX ctx;
    uint8_t ct[SHA1_SIZE];
    uint8_t digest[SHA1_SIZE];
    int res = 1;

    {
        const char *in_str = "abc";
        bigint *ct_bi = bi_str_import(bi_ctx,
                "A9993E364706816ABA3E25717850C26C9CD0D89D");
        bi_export(bi_ctx, ct_bi, ct, SHA1_SIZE);

        SHA1_Init(&ctx);
        SHA1_Update(&ctx, (const uint8_t *)in_str, strlen(in_str));
        SHA1_Final(digest, &ctx);

        if (memcmp(digest, ct, sizeof(ct)))
        {
            fprintf(stderr, "Error: SHA1 #1 failed\n");
            goto end;
        }
    }

    {
        const char *in_str =
            "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        bigint *ct_bi = bi_str_import(bi_ctx,
                "84983E441C3BD26EBAAE4AA1F95129E5E54670F1");
        bi_export(bi_ctx, ct_bi, ct, SHA1_SIZE);

        SHA1_Init(&ctx);
        SHA1_Update(&ctx, (const uint8_t *)in_str, strlen(in_str));
        SHA1_Final(digest, &ctx);

        if (memcmp(digest, ct, sizeof(ct)))
        {
            fprintf(stderr, "Error: SHA1 #2 failed\n");
            goto end;
        }
    }

    res = 0;
    printf("All SHA1 tests passed\n");

end:
    return res;
}

/**************************************************************************
 * MD5 tests 
 *
 * Run through a couple of the RFC1321 tests to verify that MD5 is correct.
 **************************************************************************/
static int MD5_test(BI_CTX *bi_ctx)
{
    MD5_CTX ctx;
    uint8_t ct[MD5_SIZE];
    uint8_t digest[MD5_SIZE];
    int res = 1;

    {
        const char *in_str =  "abc";
        bigint *ct_bi = bi_str_import(bi_ctx, 
                "900150983CD24FB0D6963F7D28E17F72");
        bi_export(bi_ctx, ct_bi, ct, MD5_SIZE);

        MD5_Init(&ctx);
        MD5_Update(&ctx, (const uint8_t *)in_str, strlen(in_str));
        MD5_Final(digest, &ctx);

        if (memcmp(digest, ct, sizeof(ct)))
        {
            fprintf(stderr, "Error: MD5 #1 failed\n");
            goto end;
        }
    }

    {
        const char *in_str =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        bigint *ct_bi = bi_str_import(
                bi_ctx, "D174AB98D277D9F5A5611C2C9F419D9F");
        bi_export(bi_ctx, ct_bi, ct, MD5_SIZE);

        MD5_Init(&ctx);
        MD5_Update(&ctx, (const uint8_t *)in_str, strlen(in_str));
        MD5_Final(digest, &ctx);

        if (memcmp(digest, ct, sizeof(ct)))
        {
            fprintf(stderr, "Error: MD5 #2 failed\n");
            goto end;
        }
    }
    res = 0;
    printf("All MD5 tests passed\n");

end:
    return res;
}

/**************************************************************************
 * HMAC tests 
 *
 * Run through a couple of the RFC2202 tests to verify that HMAC is correct.
 **************************************************************************/
static int HMAC_test(BI_CTX *bi_ctx)
{
    uint8_t key[SHA1_SIZE];
    uint8_t ct[SHA1_SIZE];
    uint8_t dgst[SHA1_SIZE];
    int res = 1;
    const char *key_str;

    const char *data_str = "Hi There";
    bigint *key_bi = bi_str_import(bi_ctx, "0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B");
    bigint *ct_bi = bi_str_import(bi_ctx, "9294727A3638BB1C13F48EF8158BFC9D");
    bi_export(bi_ctx, key_bi, key, MD5_SIZE);
    bi_export(bi_ctx, ct_bi, ct, MD5_SIZE);
    hmac_md5((const uint8_t *)data_str, 8, key, MD5_SIZE, dgst);
    if (memcmp(dgst, ct, MD5_SIZE))
    {
        printf("HMAC MD5 #1 failed\n");
        goto end;
    }

    data_str = "what do ya want for nothing?";
    key_str = "Jefe";
    ct_bi = bi_str_import(bi_ctx, "750C783E6AB0B503EAA86E310A5DB738");
    bi_export(bi_ctx, ct_bi, ct, MD5_SIZE);
    hmac_md5((const uint8_t *)data_str, 28, (const uint8_t *)key_str, 4, dgst);
    if (memcmp(dgst, ct, MD5_SIZE))
    {
        printf("HMAC MD5 #2 failed\n");
        goto end;
    }
   
    data_str = "Hi There";
    key_bi = bi_str_import(bi_ctx, "0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B");
    bi_export(bi_ctx, key_bi, key, SHA1_SIZE);
    ct_bi = bi_str_import(bi_ctx, "B617318655057264E28BC0B6FB378C8EF146BE00");
    bi_export(bi_ctx, ct_bi, ct, SHA1_SIZE);

    hmac_sha1((const uint8_t *)data_str, 8, 
            (const uint8_t *)key, SHA1_SIZE, dgst);
    if (memcmp(dgst, ct, SHA1_SIZE))
    {
        printf("HMAC SHA1 #1 failed\n");
        goto end;
    }

    data_str = "what do ya want for nothing?";
    key_str = "Jefe";
    ct_bi = bi_str_import(bi_ctx, "EFFCDF6AE5EB2FA2D27416D5F184DF9C259A7C79");
    bi_export(bi_ctx, ct_bi, ct, SHA1_SIZE);

    hmac_sha1((const uint8_t *)data_str, 28, (const uint8_t *)key_str, 5, dgst);
    if (memcmp(dgst, ct, SHA1_SIZE))
    {
        printf("HMAC SHA1 failed\n");
        exit(1);
    }

    res = 0;
    printf("All HMAC tests passed\n");

end:
    return res;
}

/**************************************************************************
 * BIGINT tests 
 *
 **************************************************************************/
static int BIGINT_test(BI_CTX *ctx)
{
    int res = 1;
    bigint *bi_data, *bi_exp, *bi_res;
    const char *expnt, *plaintext, *mod;
    uint8_t compare[MAX_KEY_BYTE_SIZE];

    /**
     * 512 bit key
     */
    plaintext = /* 64 byte number */
        "01aaaaaaaaaabbbbbbbbbbbbbbbccccccccccccccdddddddddddddeeeeeeeeee";

    mod = "C30773C8ABE09FCC279EE0E5343370DE"
          "8B2FFDB6059271E3005A7CEEF0D35E0A"
          "1F9915D95E63560836CC2EB2C289270D"
          "BCAE8CAF6F5E907FC2759EE220071E1B";

    expnt = "A1E556CD1738E10DF539E35101334E97"
          "BE8D391C57A5C89A7AD9A2EA2ACA1B3D"
          "F3140F5091CC535CBAA47CEC4159EE1F"
          "B6A3661AFF1AB758426EAB158452A9B9";

    bi_data = bi_import(ctx, (uint8_t *)plaintext, strlen(plaintext));
    bi_exp = int_to_bi(ctx, 0x10001);
    bi_set_mod(ctx, bi_str_import(ctx, mod), 0);
    bi_res = bi_mod_power(ctx, bi_data, bi_exp);

    bi_data = bi_res;   /* resuse again - see if we get the original */

    bi_exp = bi_str_import(ctx, expnt);
    bi_res = bi_mod_power(ctx, bi_data, bi_exp);
    bi_free_mod(ctx, 0);

    bi_export(ctx, bi_res, compare, 64);
    if (memcmp(plaintext, compare, 64) != 0)
        goto end;

    printf("All BIGINT tests passed\n");
    res = 0;

end:
    return res;
}

/**************************************************************************
 * RSA tests 
 *
 * Use the results from openssl to verify PKCS1 etc 
 **************************************************************************/
static int RSA_test(void)
{
    int res = 1;
    const char *plaintext = /* 128 byte hex number */
        "1aaaaaaaaaabbbbbbbbbbbbbbbccccccccccccccdddddddddddddeeeeeeeeee2"
        "1aaaaaaaaaabbbbbbbbbbbbbbbccccccccccccccdddddddddddddeeeeeeeee2\012";
    uint8_t enc_data[128], dec_data[128];
    RSA_CTX *rsa_ctx;
    BI_CTX *bi_ctx;
    bigint *plaintext_bi;
    bigint *enc_data_bi, *dec_data_bi;
    uint8_t enc_data2[128], dec_data2[128];
    int size;
    int len; 
    uint8_t *buf;

    /* extract the private key elements */
    len = get_file("../ssl/test/axTLS.key_1024", &buf);
    if (asn1_get_private_key(buf, len, &rsa_ctx) < 0)
    {
        goto end;
    }

    free(buf);
    bi_ctx = rsa_ctx->bi_ctx;
    plaintext_bi = bi_import(bi_ctx, 
            (const uint8_t *)plaintext, strlen(plaintext));

    /* basic rsa encrypt */
    enc_data_bi = RSA_public(rsa_ctx, plaintext_bi);
    bi_export(bi_ctx, bi_copy(enc_data_bi), enc_data, sizeof(enc_data));

    /* basic rsa decrypt */
    dec_data_bi = RSA_private(rsa_ctx, enc_data_bi);
    bi_export(bi_ctx, dec_data_bi, dec_data, sizeof(dec_data));

    if (memcmp(dec_data, plaintext, strlen(plaintext)))
    {
        fprintf(stderr, "Error: DECRYPT #1 failed\n");
        goto end;
    }

    RSA_encrypt(rsa_ctx, (const uint8_t *)"abc", 3, enc_data2, 0);
    size = RSA_decrypt(rsa_ctx, enc_data2, dec_data2, 1);
    if (memcmp("abc", dec_data2, 3))
    {
        fprintf(stderr, "Error: ENCRYPT/DECRYPT #2 failed\n");
        goto end;
    }

    RSA_free(rsa_ctx);
    res = 0;
    printf("All RSA tests passed\n");

end:
    return res;
}

/**************************************************************************
 * Cert Testing
 *
 **************************************************************************/
static int cert_tests(void)
{
    int res = -1, len;
    X509_CTX *x509_ctx;
    SSL_CTX *ssl_ctx;
    uint8_t *buf;

    /* check a bunch of 3rd party certificates */
    ssl_ctx = ssl_ctx_new(0, 0);
    len = get_file("../ssl/test/microsoft.x509_ca", &buf);
    if ((res = add_cert_auth(ssl_ctx, buf, len)) < 0)
    {
        printf("Cert #1\n");
        ssl_display_error(res);
        goto bad_cert;
    }

    ssl_ctx_free(ssl_ctx);
    free(buf);

    ssl_ctx = ssl_ctx_new(0, 0);
    len = get_file("../ssl/test/thawte.x509_ca", &buf);
    if ((res = add_cert_auth(ssl_ctx, buf, len)) < 0)
    {
        printf("Cert #2\n");
        ssl_display_error(res);
        goto bad_cert;
    }

    ssl_ctx_free(ssl_ctx);
    free(buf);

    ssl_ctx = ssl_ctx_new(0, 0);
    len = get_file("../ssl/test/deutsche_telecom.x509_ca", &buf);
    if ((res = add_cert_auth(ssl_ctx, buf, len)) < 0)
    {
        printf("Cert #3\n");
        ssl_display_error(res);
        goto bad_cert;
    }

    ssl_ctx_free(ssl_ctx);
    free(buf);

    ssl_ctx = ssl_ctx_new(0, 0);
    len = get_file("../ssl/test/equifax.x509_ca", &buf);
    if ((res = add_cert_auth(ssl_ctx, buf, len)) < 0)
    {
        printf("Cert #4\n");
        ssl_display_error(res);
        goto bad_cert;
    }

    ssl_ctx_free(ssl_ctx);
    free(buf);

    ssl_ctx = ssl_ctx_new(0, 0);
    len = get_file("../ssl/test/gnutls.cer", &buf);
    if ((res = add_cert(ssl_ctx, buf, len)) < 0)
    {
        printf("Cert #5\n");
        ssl_display_error(res);
        goto bad_cert;
    }

    ssl_ctx_free(ssl_ctx);
    free(buf);

    ssl_ctx = ssl_ctx_new(0, 0);
    len = get_file("../ssl/test/socgen.cer", &buf);
    if ((res = add_cert(ssl_ctx, buf, len)) < 0)
    {
        printf("Cert #6\n");
        ssl_display_error(res);
        goto bad_cert;
    }

    ssl_ctx_free(ssl_ctx);
    free(buf);

    /* Verisign use MD2 which is not supported */
    ssl_ctx = ssl_ctx_new(0, 0);
    len = get_file("../ssl/test/verisign.x509_ca", &buf);
    if ((res = add_cert_auth(ssl_ctx, buf, len)) != 
                                    X509_VFY_ERROR_UNSUPPORTED_DIGEST)
    {
        printf("Cert #7\n");
        ssl_display_error(res);
        goto bad_cert;
    }

    ssl_ctx_free(ssl_ctx);
    free(buf);

    if (get_file("../ssl/test/verisign.x509_my_cert", &buf) < 0 ||
                                    x509_new(buf, &len, &x509_ctx))
    {
        printf("Cert #8\n");
        ssl_display_error(res);
        goto bad_cert;
    }

    x509_free(x509_ctx);
    free(buf);
    res = 0;        /* all ok */
    printf("All Certificate tests passed\n");

bad_cert:
    return res;
}

/**
 * init a server socket.
 */
static int server_socket_init(int *port)
{
    struct sockaddr_in serv_addr;
    int server_fd;
    char yes = 1;

    /* Create socket for incoming connections */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        return -1;
    }
      
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

go_again:
    /* Construct local address structure */
    memset(&serv_addr, 0, sizeof(serv_addr));      /* Zero out structure */
    serv_addr.sin_family = AF_INET;                /* Internet address family */
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    serv_addr.sin_port = htons(*port);              /* Local port */

    /* Bind to the local address */
    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        (*port)++;
        goto go_again;
    }
    /* Mark the socket so it will listen for incoming connections */
    if (listen(server_fd, 3000) < 0)
    {
        return -1;
    }

    return server_fd;
}

/**
 * init a client socket.
 */
static int client_socket_init(uint16_t port)
{
    struct sockaddr_in address;
    int client_fd;

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr =  inet_addr("127.0.0.1");
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(client_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("socket");
        SOCKET_CLOSE(client_fd);
        client_fd = -1;
    }

    return client_fd;
}

/**************************************************************************
 * SSL Server Testing
 *
 **************************************************************************/
typedef struct
{
    /* not used as yet */
    int dummy;
} SVR_CTX;

typedef struct
{
    const char *testname;
    const char *openssl_option;
} client_t;

static void do_client(client_t *clnt)
{
    char openssl_buf[2048];

    /* make sure the main thread goes first */
    sleep(0);

    /* show the session ids in the reconnect test */
    if (strcmp(clnt->testname, "Session Reuse") == 0)
    {
        sprintf(openssl_buf, "echo \"hello client\" | openssl s_client "
            "-connect localhost:%d %s 2>&1 | grep \"Session-ID:\"", 
            g_port, clnt->openssl_option);
    }
    else
    {
        sprintf(openssl_buf, "echo \"hello client\" | openssl s_client "
#ifdef WIN32
            "-connect localhost:%d -quiet %s",
#else
            "-connect localhost:%d -quiet %s > /dev/null 2>&1",
#endif
        g_port, clnt->openssl_option);
    }

    system(openssl_buf);
}

static int SSL_server_test(
        SVR_CTX *svr_test_ctx,
        const char *testname, 
        const char *openssl_option, 
        const char *device_cert, 
        const char *product_cert, 
        const char *private_key,
        const char *ca_cert,
        const char *password,
        int axolotls_option)
{
    int server_fd, ret = 0;
    SSL_CTX *ssl_ctx = NULL;
    struct sockaddr_in client_addr;
    uint8_t *read_buf;
    socklen_t clnt_len = sizeof(client_addr);
    client_t client_data;
#ifndef WIN32
    pthread_t thread;
#endif
    g_port++;

    client_data.testname = testname;
    client_data.openssl_option = openssl_option;

    if ((server_fd = server_socket_init(&g_port)) < 0)
        goto error;

    if (private_key)
    {
        axolotls_option |= SSL_NO_DEFAULT_KEY;
    }

    if ((ssl_ctx = ssl_ctx_new(axolotls_option, SSL_DEFAULT_SVR_SESS)) == NULL)
    {
        ret = SSL_ERROR_INVALID_KEY;
        goto error;
    }

    if (private_key)
    {
        int obj_type = SSL_OBJ_RSA_KEY;

        if (strstr(private_key, ".p8"))
            obj_type = SSL_OBJ_PKCS8;
        else if (strstr(private_key, ".p12"))
            obj_type = SSL_OBJ_PKCS12;

        if (ssl_obj_load(ssl_ctx, obj_type, private_key, password))
        {
            ret = SSL_ERROR_INVALID_KEY;
            goto error;
        }
    }

    if (device_cert)             /* test chaining */
    {
        if ((ret = ssl_obj_load(ssl_ctx, 
                        SSL_OBJ_X509_CERT, device_cert, NULL)) != SSL_OK)
            goto error;
    }

    if (product_cert)             /* test chaining */
    {
        if ((ret = ssl_obj_load(ssl_ctx, 
                        SSL_OBJ_X509_CERT, product_cert, NULL)) != SSL_OK)
            goto error;
    }

    if (ca_cert)                  /* test adding certificate authorities */
    {
        if ((ret = ssl_obj_load(ssl_ctx, 
                        SSL_OBJ_X509_CACERT, ca_cert, NULL)) != SSL_OK)
            goto error;
    }

#ifndef WIN32
    pthread_create(&thread, NULL, 
                (void *(*)(void *))do_client, (void *)&client_data);
    pthread_detach(thread);
#else
    CreateThread(NULL, 1024, (LPTHREAD_START_ROUTINE)do_client, 
            (LPVOID)&client_data, 0, NULL);
#endif

    for (;;)
    {
        int client_fd, size = 0; 
        SSL *ssl;

        /* Wait for a client to connect */
        if ((client_fd = accept(server_fd, 
                        (struct sockaddr *)&client_addr, &clnt_len)) < 0)
        {
            ret = SSL_ERROR_SOCK_SETUP_FAILURE;
            goto error;
        }
        
        /* we are ready to go */
        ssl = ssl_server_new(ssl_ctx, client_fd);
        while ((size = ssl_read(ssl, &read_buf)) == SSL_OK);
        SOCKET_CLOSE(client_fd);
        
        if (size < SSL_OK) /* got some alert or something nasty */
        {
            ret = size;

            if (ret == SSL_ERROR_CONN_LOST)
            {
                ret = SSL_OK;
                continue;
            }

            break;  /* we've got a problem */
        }
        else /* looks more promising */
        {
            if (strstr("hello client", (char *)read_buf) == NULL)
            {
                printf("SSL server test \"%s\" passed\n", testname);
                TTY_FLUSH();
                ret = 0;
                break;
            }
        }

        ssl_free(ssl);
    }

    SOCKET_CLOSE(server_fd);

error:
    ssl_ctx_free(ssl_ctx);
    return ret;
}

int SSL_server_tests(void)
{
    int ret = -1;
    struct stat stat_buf;
    SVR_CTX svr_test_ctx;
    memset(&svr_test_ctx, 0, sizeof(SVR_CTX));

    printf("### starting server tests\n");

    /* Go through the algorithms */

    /* 
     * TLS1 client hello 
     */
    if ((ret = SSL_server_test(NULL, "TLSv1", "-cipher RC4-SHA -tls1", 
                    NULL, NULL, NULL, NULL, NULL, DEFAULT_SVR_OPTION)))
        goto cleanup;

    /*
     * AES128-SHA
     */
    if ((ret = SSL_server_test(NULL, "AES256-SHA", "-cipher AES128-SHA", 
                    DEFAULT_CERT, NULL, DEFAULT_KEY, NULL, NULL,
                    DEFAULT_SVR_OPTION)))
        goto cleanup;

    /*
     * AES256-SHA
     */
    if ((ret = SSL_server_test(NULL, "AES256-SHA", "-cipher AES128-SHA", 
                    DEFAULT_CERT, NULL, DEFAULT_KEY, NULL, NULL,
                    DEFAULT_SVR_OPTION)))
        goto cleanup;

    /*
     * RC4-SHA
     */
    if ((ret = SSL_server_test(NULL, "RC4-SHA", "-cipher RC4-SHA", 
                DEFAULT_CERT, NULL, DEFAULT_KEY, NULL, NULL,
                DEFAULT_SVR_OPTION)))
        goto cleanup;

    /*
     * RC4-MD5
     */
    if ((ret = SSL_server_test(NULL, "RC4-MD5", "-cipher RC4-MD5", 
                DEFAULT_CERT, NULL, DEFAULT_KEY, NULL, NULL,
                DEFAULT_SVR_OPTION)))
        goto cleanup;

    /*
     * Session Reuse
     * all the session id's should match for session resumption.
     */
    if ((ret = SSL_server_test(NULL, "Session Reuse", 
                    "-cipher RC4-SHA -reconnect", 
                    DEFAULT_CERT, NULL, DEFAULT_KEY, NULL, NULL,
                    DEFAULT_SVR_OPTION)))
        goto cleanup;

    /* 
     * 512 bit RSA key 
     */
    if ((ret = SSL_server_test(NULL, "512 bit key", "-cipher RC4-SHA", 
                    "../ssl/test/axTLS.x509_512.cer", NULL, 
                    "../ssl/test/axTLS.key_512",
                    NULL, NULL, DEFAULT_SVR_OPTION)))
        goto cleanup;

    /* 
     * 1024 bit RSA key (check certificate chaining)
     */
    if ((ret = SSL_server_test(NULL, "1024 bit key", 
                    "-cipher RC4-SHA", 
                    "../ssl/test/axTLS.x509_device.cer", 
                    "../ssl/test/axTLS.x509_512.cer", 
                    "../ssl/test/axTLS.device_key",
                    NULL, NULL, DEFAULT_SVR_OPTION)))
        goto cleanup;

    /* 
     * 2048 bit RSA key 
     */
    if ((ret = SSL_server_test(NULL, "2048 bit key", 
                    "-cipher RC4-SHA",
                    "../ssl/test/axTLS.x509_2048.cer", NULL, 
                    "../ssl/test/axTLS.key_2048",
                    NULL, NULL, DEFAULT_SVR_OPTION)))
        goto cleanup;

    /* 
     * 4096 bit RSA key 
     */
    if ((ret = SSL_server_test(NULL, "4096 bit key", 
                    "-cipher RC4-SHA",
                    "../ssl/test/axTLS.x509_4096.cer", NULL, 
                    "../ssl/test/axTLS.key_4096",
                    NULL, NULL, DEFAULT_SVR_OPTION)))
        goto cleanup;

    /* 
     * Client Verification
     */
    if ((ret = SSL_server_test(NULL, "Client Verification", 
                    "-cipher RC4-SHA -tls1 "
                    "-cert ../ssl/test/axTLS.x509_2048.pem "
                    "-key ../ssl/test/axTLS.key_2048.pem ",
                    NULL, NULL, NULL, 
                    "../ssl/test/axTLS.ca_x509.cer", NULL,
                    DEFAULT_SVR_OPTION|SSL_CLIENT_AUTHENTICATION)))
        goto cleanup;

    /* this test should fail */
    if (stat("../ssl/test/axTLS.x509_bad_before.pem", &stat_buf) >= 0)
    {
        if ((ret = SSL_server_test(NULL, "Bad Before Cert", 
                    "-cipher RC4-SHA -tls1 "
                    "-cert ../ssl/test/axTLS.x509_bad_before.pem "
                    "-key ../ssl/test/axTLS.key_512.pem ",
                    NULL, NULL, NULL, 
                    "../ssl/test/axTLS.ca_x509.cer", NULL,
                    DEFAULT_SVR_OPTION|SSL_CLIENT_AUTHENTICATION)) !=
                            SSL_X509_ERROR(X509_VFY_ERROR_NOT_YET_VALID))
            goto cleanup;

        printf("SSL server test \"%s\" passed\n", "Bad Before Cert");
        TTY_FLUSH();
        ret = 0;    /* is ok */
    }

    /* this test should fail */
    if ((ret = SSL_server_test(NULL, "Bad After Cert", 
                    "-cipher RC4-SHA -tls1 "
                    "-cert ../ssl/test/axTLS.x509_bad_after.pem "
                    "-key ../ssl/test/axTLS.key_512.pem ",
                    NULL, NULL, NULL, 
                    "../ssl/test/axTLS.ca_x509.cer", NULL,
                    DEFAULT_SVR_OPTION|SSL_CLIENT_AUTHENTICATION)) !=
                            SSL_X509_ERROR(X509_VFY_ERROR_EXPIRED))
        goto cleanup;

    printf("SSL server test \"%s\" passed\n", "Bad After Cert");
    TTY_FLUSH();

    /* 
     * Key in PEM format
     */
    if ((ret = SSL_server_test(NULL, "Key in PEM format",
                    "-cipher RC4-SHA", 
                    "../ssl/test/axTLS.x509_512.cer", NULL, 
                    "../ssl/test/axTLS.key_512.pem", NULL,
                    NULL, DEFAULT_SVR_OPTION)))
        goto cleanup;

    /* 
     * Cert in PEM format
     */
    if ((ret = SSL_server_test(NULL, "Cert in PEM format", 
                    "-cipher RC4-SHA", 
                    "../ssl/test/axTLS.x509_512.pem", NULL, 
                    "../ssl/test/axTLS.key_512.pem", NULL,
                    NULL, DEFAULT_SVR_OPTION)))
        goto cleanup;

    /* 
     * Cert chain in PEM format
     */
    if ((ret = SSL_server_test(NULL, "Cert chain in PEM format", 
                    "-cipher RC4-SHA", 
                    "../ssl/test/axTLS.x509_device.pem", 
                    NULL, "../ssl/test/axTLS.device_key.pem",
                    "../ssl/test/axTLS.ca_x509.pem", NULL, DEFAULT_SVR_OPTION)))
        goto cleanup;

    /* 
     * AES128 Encrypted key 
     */
    if ((ret = SSL_server_test(NULL, "AES128 encrypted key", 
                    "-cipher RC4-SHA", 
                    "../ssl/test/axTLS.x509_aes128.pem", NULL, 
                    "../ssl/test/axTLS.key_aes128.pem",
                    NULL, "abcd", DEFAULT_SVR_OPTION)))
        goto cleanup;

    /* 
     * AES256 Encrypted key 
     */
    if ((ret = SSL_server_test(NULL, "AES256 encrypted key", 
                    "-cipher RC4-SHA", 
                    "../ssl/test/axTLS.x509_aes256.pem", NULL, 
                    "../ssl/test/axTLS.key_aes256.pem",
                    NULL, "abcd", DEFAULT_SVR_OPTION)))
        goto cleanup;

    /* 
     * AES128 Encrypted invalid key 
     */
    if ((ret = SSL_server_test(NULL, "AES128 encrypted invalid key", 
                    "-cipher RC4-SHA", 
                    "../ssl/test/axTLS.x509_aes128.pem", NULL, 
                    "../ssl/test/axTLS.key_aes128.pem",
                    NULL, "xyz", DEFAULT_SVR_OPTION)) != SSL_ERROR_INVALID_KEY)
        goto cleanup;

    printf("SSL server test \"%s\" passed\n", "AES128 encrypted invalid key");
    TTY_FLUSH();

    /*
     * PKCS#8 key (encrypted)
     */
    if ((ret = SSL_server_test(NULL, "pkcs#8 encrypted", "-cipher RC4-SHA", 
                DEFAULT_CERT, NULL, "../ssl/test/axTLS.encrypted.p8", 
                NULL, "abcd", DEFAULT_SVR_OPTION)))
        goto cleanup;

    /*
     * PKCS#8 key (unencrypted)
     */
    if ((ret = SSL_server_test(NULL, "pkcs#8 unencrypted", "-cipher RC4-SHA", 
                DEFAULT_CERT, NULL, "../ssl/test/axTLS.unencrypted.p8", 
                NULL, NULL, DEFAULT_SVR_OPTION)))
        goto cleanup;

    /*
     * PKCS#12 key/certificate
     */
    if ((ret = SSL_server_test(NULL, "pkcs#12 with CA", "-cipher RC4-SHA", 
                NULL, NULL, "../ssl/test/axTLS.withCA.p12", 
                NULL, "abcd", DEFAULT_SVR_OPTION)))
        goto cleanup;

    if ((ret = SSL_server_test(NULL, "pkcs#12 no CA", "-cipher RC4-SHA", 
                DEFAULT_CERT, NULL, "../ssl/test/axTLS.withoutCA.p12", 
                NULL, "abcd", DEFAULT_SVR_OPTION)))
        goto cleanup;

    ret = 0;

cleanup:
    if (ret)
        fprintf(stderr, "Error: A server test failed\n");
    return ret;
}

/**************************************************************************
 * SSL Client Testing
 *
 **************************************************************************/
typedef struct
{
    uint8_t session_id[SSL_SESSION_ID_SIZE];
#ifndef WIN32
    pthread_t server_thread;
#endif
    int start_server;
    int stop_server;
    int do_reneg;
} CLNT_SESSION_RESUME_CTX;

typedef struct
{
    const char *testname;
    const char *openssl_option;
} server_t;

static void do_server(server_t *svr)
{
    char openssl_buf[2048];
#ifndef WIN32
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
#endif
    sprintf(openssl_buf, "openssl s_server -tls1 " 
            "-accept %d -quiet %s ", g_port, svr->openssl_option);
    system(openssl_buf);
}

static int SSL_client_test(
        const char *test,
        SSL_CTX **ssl_ctx,
        const char *openssl_option, 
        CLNT_SESSION_RESUME_CTX *sess_resume,
        uint32_t client_options,
        const char *private_key,
        const char *password,
        const char *cert)
{
    server_t server_data;
    SSL *ssl = NULL;
    int client_fd = -1;
    uint8_t *session_id = NULL;
    int ret = 1;
#ifndef WIN32
    pthread_t thread;
#endif

    if (sess_resume == NULL || sess_resume->start_server)
    {
        g_port++;
        server_data.openssl_option = openssl_option;

#ifndef WIN32
        pthread_create(&thread, NULL, 
                (void *(*)(void *))do_server, (void *)&server_data);
        pthread_detach(thread);
#else
        CreateThread(NULL, 1024, (LPTHREAD_START_ROUTINE)do_server, 
            (LPVOID)&server_data, 0, NULL);
#endif
    }
    
    usleep(200000);           /* allow server to start */

    if (*ssl_ctx == NULL)
    {
        if (private_key)
        {
            client_options |= SSL_NO_DEFAULT_KEY;
        }

        if ((*ssl_ctx = ssl_ctx_new(
                            client_options, SSL_DEFAULT_CLNT_SESS)) == NULL)
        {
            ret = SSL_ERROR_INVALID_KEY;
            goto client_test_exit;
        }

        if (private_key)
        {
            int obj_type = SSL_OBJ_RSA_KEY;

            if (strstr(private_key, ".p8"))
                obj_type = SSL_OBJ_PKCS8;
            else if (strstr(private_key, ".p12"))
                obj_type = SSL_OBJ_PKCS12;

            if (ssl_obj_load(*ssl_ctx, obj_type, private_key, password))
            {
                ret = SSL_ERROR_INVALID_KEY;
                goto client_test_exit;
            }
        }

        if (cert)                  
        {
            if ((ret = ssl_obj_load(*ssl_ctx, 
                            SSL_OBJ_X509_CERT, cert, NULL)) != SSL_OK)
            {
                printf("could not add cert %s (%d)\n", cert, ret);
                TTY_FLUSH();
                goto client_test_exit;
            }
        }
    }
    
    if (sess_resume && !sess_resume->start_server) 
    {
        session_id = sess_resume->session_id;
    }

    if ((client_fd = client_socket_init(g_port)) < 0)
    {
        printf("could not start socket on %d\n", g_port);
        TTY_FLUSH();
        goto client_test_exit;
    }

    if (ssl_obj_load(*ssl_ctx, SSL_OBJ_X509_CACERT, 
                                        "../ssl/test/axTLS.ca_x509.cer", NULL))
    {
        printf("could not add cert auth\n");
        TTY_FLUSH();
        goto client_test_exit;
    }

    ssl = ssl_client_new(*ssl_ctx, client_fd, session_id, sizeof(session_id));

    /* check the return status */
    if ((ret = ssl_handshake_status(ssl)))
        goto client_test_exit;

    /* renegotiate client */
    if (sess_resume && sess_resume->do_reneg) 
    {
        if (ssl_renegotiate(ssl) < 0)
            goto client_test_exit;
    }

    if (sess_resume)
    {
        memcpy(sess_resume->session_id, 
                ssl_get_session_id(ssl), SSL_SESSION_ID_SIZE);
    }

    if (IS_SET_SSL_FLAG(SSL_SERVER_VERIFY_LATER) && 
                                            (ret = ssl_verify_cert(ssl)))
    {
        goto client_test_exit;
    }

    ssl_write(ssl, (uint8_t *)"hello world\n", 13);
    if (sess_resume)
    {
        const uint8_t *sess_id = ssl_get_session_id(ssl);
        int i;

        printf("    Session-ID: ");
        for (i = 0; i < SSL_SESSION_ID_SIZE; i++)
        {
            printf("%02X", sess_id[i]);
        }
        printf("\n");
        TTY_FLUSH();
    }

    ret = 0;

client_test_exit:
    ssl_free(ssl);
    SOCKET_CLOSE(client_fd);
    usleep(200000);           /* allow openssl to say something */

    if (sess_resume)
    {
        if (sess_resume->stop_server)
        {
            ssl_ctx_free(*ssl_ctx);
            *ssl_ctx = NULL;
#ifndef WIN32
            pthread_cancel(sess_resume->server_thread);
#endif
        }
        else if (sess_resume->start_server)
        {
#ifndef WIN32
           sess_resume->server_thread = thread;
#endif
        }
    }
    else
    {
        ssl_ctx_free(*ssl_ctx);
        *ssl_ctx = NULL;
#ifndef WIN32
        pthread_cancel(thread);
#endif
    }

    if (ret == 0)
    {
        printf("SSL client test \"%s\" passed\n", test);
        TTY_FLUSH();
    }

    return ret;
}

int SSL_client_tests(void)
{
    int ret =  -1;
    SSL_CTX *ssl_ctx = NULL;
    CLNT_SESSION_RESUME_CTX sess_resume;
    memset(&sess_resume, 0, sizeof(CLNT_SESSION_RESUME_CTX));

    sess_resume.start_server = 1;
    printf("### starting client tests\n");
   
    if ((ret = SSL_client_test("512 bit key", 
                    &ssl_ctx,
                    "-cert ../ssl/test/axTLS.x509_512.pem "
                    "-key ../ssl/test/axTLS.key_512.pem", &sess_resume, 
                    DEFAULT_CLNT_OPTION, NULL, NULL, NULL)))
        goto cleanup;

    /* all the session id's should match for session resumption */
    sess_resume.start_server = 0;
    if ((ret = SSL_client_test("Client session resumption #1", 
                    &ssl_ctx, NULL, &sess_resume, 
                    DEFAULT_CLNT_OPTION, NULL, NULL, NULL)))
        goto cleanup;

    sess_resume.do_reneg = 1;
    if ((ret = SSL_client_test("Client renegotiation", 
                    &ssl_ctx, NULL, &sess_resume, 
                    DEFAULT_CLNT_OPTION, NULL, NULL, NULL)))
        goto cleanup;
    sess_resume.do_reneg = 0;

    sess_resume.stop_server = 1;
    if ((ret = SSL_client_test("Client session resumption #2", 
                    &ssl_ctx, NULL, &sess_resume, 
                    DEFAULT_CLNT_OPTION, NULL, NULL, NULL)))
        goto cleanup;

    if ((ret = SSL_client_test("1024 bit key", 
                    &ssl_ctx,
                    "-cert ../ssl/test/axTLS.x509_1024.pem "
                    "-key ../ssl/test/axTLS.key_1024.pem", NULL,
                    DEFAULT_CLNT_OPTION, NULL, NULL, NULL)))
        goto cleanup;

    if ((ret = SSL_client_test("2048 bit key", 
                    &ssl_ctx,
                    "-cert ../ssl/test/axTLS.x509_2048.pem "
                    "-key ../ssl/test/axTLS.key_2048.pem",  NULL,
                    DEFAULT_CLNT_OPTION, NULL, NULL, NULL)))
        goto cleanup;

    if ((ret = SSL_client_test("4096 bit key", 
                    &ssl_ctx,
                    "-cert ../ssl/test/axTLS.x509_4096.pem "
                    "-key ../ssl/test/axTLS.key_4096.pem", NULL,
                    DEFAULT_CLNT_OPTION, NULL, NULL, NULL)))
        goto cleanup;

    if ((ret = SSL_client_test("Server cert chaining", 
                    &ssl_ctx,
                    "-cert ../ssl/test/axTLS.x509_device.pem "
                    "-key ../ssl/test/axTLS.device_key.pem "
                    "-CAfile ../ssl/test/axTLS.x509_512.pem", NULL,
                    DEFAULT_CLNT_OPTION, NULL, NULL, NULL)))
        goto cleanup;

    /* Check the server can verify the client */
    if ((ret = SSL_client_test("Client peer authentication",
                    &ssl_ctx,
                    "-cert ../ssl/test/axTLS.x509_2048.pem "
                    "-key ../ssl/test/axTLS.key_2048.pem "
                    "-CAfile ../ssl/test/axTLS.ca_x509.pem "
                    "-verify 1 ", NULL, DEFAULT_CLNT_OPTION, 
                    "../ssl/test/axTLS.key_1024", NULL,
                    "../ssl/test/axTLS.x509_1024.cer")))
        goto cleanup;

    /* Should get an "ERROR" from openssl (as the handshake fails as soon as
     * the certificate verification fails) */
    if ((ret = SSL_client_test("Expired cert (verify now) should fail!",
                    &ssl_ctx,
                    "-cert ../ssl/test/axTLS.x509_bad_after.pem "
                    "-key ../ssl/test/axTLS.key_512.pem", NULL,
                    DEFAULT_CLNT_OPTION, NULL, NULL, NULL)) != 
                            SSL_X509_ERROR(X509_VFY_ERROR_EXPIRED))
    {
        printf("*** Error: %d\n", ret);
        goto cleanup;
    }

    printf("SSL client test \"Expired cert (verify now)\" passed\n");
    ret = 0;

    /* There is no "ERROR" from openssl */
    if ((ret = SSL_client_test("Expired cert (verify later) should fail!", 
                    &ssl_ctx,
                    "-cert ../ssl/test/axTLS.x509_bad_after.pem "
                    "-key ../ssl/test/axTLS.key_512.pem", NULL,
                    DEFAULT_CLNT_OPTION|SSL_SERVER_VERIFY_LATER, NULL, 
                    NULL, NULL)) != SSL_X509_ERROR(X509_VFY_ERROR_EXPIRED))
    {
        printf("*** Error: %d\n", ret);
        goto cleanup;
    }

    printf("SSL client test \"Expired cert (verify later)\" passed\n");

    ret = 0;

cleanup:
    if (ret)
        fprintf(stderr, "Error: A client test failed\n");

    return ret;
}

/**************************************************************************
 * SSL Basic Testing (test a big packet handshake)
 *
 **************************************************************************/
static uint8_t basic_buf[256*1024];

static void do_basic(void)
{
    int client_fd;
    SSL *ssl_clnt;
    SSL_CTX *ssl_clnt_ctx = ssl_ctx_new(
                            DEFAULT_CLNT_OPTION, SSL_DEFAULT_CLNT_SESS);
    usleep(200000);           /* allow server to start */

    if ((client_fd = client_socket_init(g_port)) < 0)
        goto error;

    if (ssl_obj_load(ssl_clnt_ctx, SSL_OBJ_X509_CACERT, 
                                        "../ssl/test/axTLS.ca_x509.cer", NULL))
        goto error;

    ssl_clnt = ssl_client_new(ssl_clnt_ctx, client_fd, NULL, 0);

    /* check the return status */
    if (ssl_handshake_status(ssl_clnt))
    {
        printf("Client ");
        ssl_display_error(ssl_handshake_status(ssl_clnt));
        goto error;
    }

    ssl_write(ssl_clnt, basic_buf, sizeof(basic_buf));
    ssl_free(ssl_clnt);

error:
    ssl_ctx_free(ssl_clnt_ctx);
    SOCKET_CLOSE(client_fd);

    /* exit this thread */
}

static int SSL_basic_test(void)
{
    int server_fd, client_fd, ret = 0, size = 0, offset = 0;
    SSL_CTX *ssl_svr_ctx = NULL;
    struct sockaddr_in client_addr;
    uint8_t *read_buf;
    socklen_t clnt_len = sizeof(client_addr);
    SSL *ssl_svr;
#ifndef WIN32
    pthread_t thread;
#endif
    memset(basic_buf, 0xA5, sizeof(basic_buf)/2);
    memset(&basic_buf[sizeof(basic_buf)/2], 0x5A, sizeof(basic_buf)/2);

    if ((server_fd = server_socket_init(&g_port)) < 0)
        goto error;

    ssl_svr_ctx = ssl_ctx_new(DEFAULT_SVR_OPTION, SSL_DEFAULT_SVR_SESS);

#ifndef WIN32
    pthread_create(&thread, NULL, 
                (void *(*)(void *))do_basic, NULL);
    pthread_detach(thread);
#else
    CreateThread(NULL, 1024, (LPTHREAD_START_ROUTINE)do_basic, NULL, 0, NULL);
#endif

    /* Wait for a client to connect */
    if ((client_fd = accept(server_fd, 
                    (struct sockaddr *) &client_addr, &clnt_len)) < 0)
    {
        ret = SSL_ERROR_SOCK_SETUP_FAILURE;
        goto error;
    }
    
    /* we are ready to go */
    ssl_svr = ssl_server_new(ssl_svr_ctx, client_fd);
    
    do
    {
        while ((size = ssl_read(ssl_svr, &read_buf)) == SSL_OK);

        if (size < SSL_OK) /* got some alert or something nasty */
        {
            printf("Server ");
            ssl_display_error(size);
            ret = size;
            break;
        }
        else /* looks more promising */
        {
            if (memcmp(read_buf, &basic_buf[offset], size) != 0)
            {
                ret = SSL_NOT_OK;
                break;
            }
        }

        offset += size;
    } while (offset < sizeof(basic_buf));

    printf(ret == SSL_OK && offset == sizeof(basic_buf) ? 
                            "SSL basic test passed\n" :
                            "SSL basic test failed\n");
    TTY_FLUSH();

    ssl_free(ssl_svr);
    SOCKET_CLOSE(server_fd);
    SOCKET_CLOSE(client_fd);

error:
    ssl_ctx_free(ssl_svr_ctx);
    return ret;
}

#if !defined(WIN32) && defined(CONFIG_SSL_CTX_MUTEXING)
/**************************************************************************
 * Multi-Threading Tests
 *
 **************************************************************************/
#define NUM_THREADS         100

typedef struct
{
    SSL_CTX *ssl_clnt_ctx;
    int port;
    int thread_id;
} multi_t;

void do_multi_clnt(multi_t *multi_data)
{
    int res = 1, client_fd, i;
    SSL *ssl = NULL;
    char tmp[5];

    if ((client_fd = client_socket_init(multi_data->port)) < 0)
        goto client_test_exit;

    sleep(1);
    ssl = ssl_client_new(multi_data->ssl_clnt_ctx, client_fd, NULL, 0);

    if ((res = ssl_handshake_status(ssl)))
    {
        printf("Client ");
        ssl_display_error(res);
        goto client_test_exit;
    }

    sprintf(tmp, "%d\n", multi_data->thread_id);
    for (i = 0; i < 10; i++)
        ssl_write(ssl, (uint8_t *)tmp, strlen(tmp)+1);

client_test_exit:
    ssl_free(ssl);
    SOCKET_CLOSE(client_fd);
    free(multi_data);
}

void do_multi_svr(SSL *ssl)
{
    uint8_t *read_buf;
    int *res_ptr = malloc(sizeof(int));
    int res;

    for (;;)
    {
        res = ssl_read(ssl, &read_buf);

        /* kill the client */
        if (res != SSL_OK)
        {
            if (res == SSL_ERROR_CONN_LOST)
            {
                SOCKET_CLOSE(ssl->client_fd);
                ssl_free(ssl);
                break;
            }
            else if (res > 0)
            {
                /* do nothing */
            }
            else /* some problem */
            {
                printf("Server ");
                ssl_display_error(res);
                goto error;
            }
        }
    }

    res = SSL_OK;
error:
    *res_ptr = res;
    pthread_exit(res_ptr);
}

int multi_thread_test(void)
{
    int server_fd = -1;
    SSL_CTX *ssl_server_ctx;
    SSL_CTX *ssl_clnt_ctx;
    pthread_t clnt_threads[NUM_THREADS];
    pthread_t svr_threads[NUM_THREADS];
    int i, res = 0;
    struct sockaddr_in client_addr;
    socklen_t clnt_len = sizeof(client_addr);

    printf("Do multi-threading test (takes a minute)\n");

    ssl_server_ctx = ssl_ctx_new(DEFAULT_SVR_OPTION, SSL_DEFAULT_SVR_SESS);
    ssl_clnt_ctx = ssl_ctx_new(DEFAULT_CLNT_OPTION, SSL_DEFAULT_CLNT_SESS);

    if (ssl_obj_load(ssl_clnt_ctx, SSL_OBJ_X509_CACERT, 
                                        "../ssl/test/axTLS.ca_x509.cer", NULL))
        goto error;

    if ((server_fd = server_socket_init(&g_port)) < 0)
        goto error;

    for (i = 0; i < NUM_THREADS; i++)
    {
        multi_t *multi_data = (multi_t *)malloc(sizeof(multi_t));
        multi_data->ssl_clnt_ctx = ssl_clnt_ctx;
        multi_data->port = g_port;
        multi_data->thread_id = i+1;
        pthread_create(&clnt_threads[i], NULL, 
                (void *(*)(void *))do_multi_clnt, (void *)multi_data);
        pthread_detach(clnt_threads[i]);
    }

    for (i = 0; i < NUM_THREADS; i++)
    { 
        SSL *ssl_svr;
        int client_fd = accept(server_fd, 
                      (struct sockaddr *)&client_addr, &clnt_len);

        if (client_fd < 0)
            goto error;

        ssl_svr = ssl_server_new(ssl_server_ctx, client_fd);

        pthread_create(&svr_threads[i], NULL, 
                        (void *(*)(void *))do_multi_svr, (void *)ssl_svr);
    }

    /* make sure we've run all of the threads */
    for (i = 0; i < NUM_THREADS; i++)
    {
        void *thread_res;
        pthread_join(svr_threads[i], &thread_res);

        if (*((int *)thread_res) != 0)
            res = 1;

        free(thread_res);
    } 

    if (res) 
        goto error;

    printf("Multi-thread test passed (%d)\n", NUM_THREADS);
error:
    ssl_ctx_free(ssl_server_ctx);
    ssl_ctx_free(ssl_clnt_ctx);
    SOCKET_CLOSE(server_fd);
    return res;
}
#endif

/**************************************************************************
 * Header issue
 *
 **************************************************************************/
static void do_header_issue(void)
{
    uint8_t axtls_buf[2048];
#ifndef WIN32
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
#endif
    sprintf(axtls_buf, "./axssl s_client -connect localhost:%d", g_port);
    system(axtls_buf);
}

static int header_issue(void)
{
    FILE *f = fopen("../ssl/test/header_issue.dat", "r");
    int server_fd, client_fd, ret = 1;
    uint8_t buf[2048];
    int size = 0;
    struct sockaddr_in client_addr;
    socklen_t clnt_len = sizeof(client_addr);
#ifndef WIN32
    pthread_t thread;
#endif

    if (f == NULL || (server_fd = server_socket_init(&g_port)) < 0)
        goto error;

#ifndef WIN32
    pthread_create(&thread, NULL, 
                (void *(*)(void *))do_header_issue, NULL);
    pthread_detach(thread);
#else
    CreateThread(NULL, 1024, (LPTHREAD_START_ROUTINE)do_header_issue, 
                NULL, 0, NULL);
#endif
    if ((client_fd = accept(server_fd, 
                    (struct sockaddr *) &client_addr, &clnt_len)) < 0)
    {
        ret = SSL_ERROR_SOCK_SETUP_FAILURE;
        goto error;
    }

    size = fread(buf, 1, sizeof(buf), f);
    SOCKET_WRITE(client_fd, buf, size);
    usleep(200000);

    ret = 0;
error:
    fclose(f);
    SOCKET_CLOSE(client_fd);
    SOCKET_CLOSE(server_fd);
    TTY_FLUSH();
    system("killall axssl");
    return ret;
}

/**************************************************************************
 * main()
 *
 **************************************************************************/
int main(int argc, char *argv[])
{
    int ret = 1;
    BI_CTX *bi_ctx;
    int fd;

#ifdef WIN32
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSAStartup(wVersionRequested, &wsaData);
    fd = _open("test_result.txt", O_WRONLY|O_TEMPORARY|O_CREAT, _S_IWRITE);
    dup2(fd, 2);                        /* write stderr to this file */
#else
    fd = open("/dev/null", O_WRONLY);   /* write stderr to /dev/null */
    signal(SIGPIPE, SIG_IGN);           /* ignore pipe errors */
    dup2(fd, 2);
#endif

    bi_ctx = bi_initialize();

    if (AES_test(bi_ctx))
    {
        printf("AES tests failed\n");
        goto cleanup;
    }
    TTY_FLUSH();

    if (RC4_test(bi_ctx))
    {
        printf("RC4 tests failed\n");
        goto cleanup;
    }
    TTY_FLUSH();

    if (MD5_test(bi_ctx))
    {
        printf("MD5 tests failed\n");
        goto cleanup;
    }
    TTY_FLUSH();

    if (SHA1_test(bi_ctx))
    {
        printf("SHA1 tests failed\n");
        goto cleanup;
    }
    TTY_FLUSH();

    if (HMAC_test(bi_ctx))
    {
        printf("HMAC tests failed\n");
        goto cleanup;
    }
    TTY_FLUSH();

    if (BIGINT_test(bi_ctx))
    {
        printf("BigInt tests failed!\n");
        goto cleanup;
    }
    TTY_FLUSH();

    bi_terminate(bi_ctx);

    if (RSA_test())
    {
        printf("RSA tests failed\n");
        goto cleanup;
    }
    TTY_FLUSH();

    if (cert_tests())
    {
        printf("CERT tests failed\n");
        goto cleanup;
    }
    TTY_FLUSH();

#if !defined(WIN32) && defined(CONFIG_SSL_CTX_MUTEXING)
    if (multi_thread_test())
        goto cleanup;
#endif

    if (SSL_basic_test())
        goto cleanup;

    system("sh ../ssl/test/killopenssl.sh");

    if (SSL_client_tests())
        goto cleanup;

    system("sh ../ssl/test/killopenssl.sh");

    if (SSL_server_tests())
        goto cleanup;

    system("sh ../ssl/test/killopenssl.sh");

    if (header_issue())
    {
        printf("Header tests failed\n");
        goto cleanup;
    }

    ret = 0;        /* all ok */
    printf("**** ALL TESTS PASSED ****\n"); TTY_FLUSH();
cleanup:

    if (ret)
    {
        fprintf(stderr, "Error: Some tests failed!\n");
    }

    close(fd);
    return ret;
}
