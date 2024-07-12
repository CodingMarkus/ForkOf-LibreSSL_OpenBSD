/* $OpenBSD: x509_v3.c,v 1.37 2024/07/12 09:33:05 tb Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>

#include <openssl/asn1.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/stack.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "x509_local.h"

int
X509v3_get_ext_count(const STACK_OF(X509_EXTENSION) *sk)
{
	if (sk == NULL)
		return 0;

	return sk_X509_EXTENSION_num(sk);
}
LCRYPTO_ALIAS(X509v3_get_ext_count);

int
X509v3_get_ext_by_NID(const STACK_OF(X509_EXTENSION) *sk, int nid, int lastpos)
{
	const ASN1_OBJECT *obj;

	if ((obj = OBJ_nid2obj(nid)) == NULL)
		return -2;

	return X509v3_get_ext_by_OBJ(sk, obj, lastpos);
}
LCRYPTO_ALIAS(X509v3_get_ext_by_NID);

int
X509v3_get_ext_by_OBJ(const STACK_OF(X509_EXTENSION) *sk,
    const ASN1_OBJECT *obj, int lastpos)
{
	if (++lastpos < 0)
		lastpos = 0;

	for (; lastpos < X509v3_get_ext_count(sk); lastpos++) {
		const X509_EXTENSION *ext = X509v3_get_ext(sk, lastpos);

		if (OBJ_cmp(ext->object, obj) == 0)
			return lastpos;
	}

	return -1;
}
LCRYPTO_ALIAS(X509v3_get_ext_by_OBJ);

int
X509v3_get_ext_by_critical(const STACK_OF(X509_EXTENSION) *sk, int crit,
    int lastpos)
{
	crit = (crit != 0);

	if (++lastpos < 0)
		lastpos = 0;

	for (; lastpos < X509v3_get_ext_count(sk); lastpos++) {
		const X509_EXTENSION *ext = X509v3_get_ext(sk, lastpos);

		if (X509_EXTENSION_get_critical(ext) == crit)
			return lastpos;
	}

	return -1;
}
LCRYPTO_ALIAS(X509v3_get_ext_by_critical);

X509_EXTENSION *
X509v3_get_ext(const STACK_OF(X509_EXTENSION) *sk, int loc)
{
	return sk_X509_EXTENSION_value(sk, loc);
}
LCRYPTO_ALIAS(X509v3_get_ext);

X509_EXTENSION *
X509v3_delete_ext(STACK_OF(X509_EXTENSION) *sk, int loc)
{
	return sk_X509_EXTENSION_delete(sk, loc);
}
LCRYPTO_ALIAS(X509v3_delete_ext);

STACK_OF(X509_EXTENSION) *
X509v3_add_ext(STACK_OF(X509_EXTENSION) **x, X509_EXTENSION *ext, int loc)
{
	STACK_OF(X509_EXTENSION) *sk = NULL;
	X509_EXTENSION *new_ext = NULL;

	/*
	 * XXX - Nonsense from the poorly reviewed OpenSSL c755c5fd8ba (2005).
	 * This check should have been joined with the next check, i.e., if no
	 * stack was passed in, a new one should be created and returned.
	 */
	if (x == NULL) {
		X509error(ERR_R_PASSED_NULL_PARAMETER);
		goto err;
	}

	if ((sk = *x) == NULL)
		sk = sk_X509_EXTENSION_new_null();
	if (sk == NULL) {
		X509error(ERR_R_MALLOC_FAILURE);
		goto err;
	}

	if ((new_ext = X509_EXTENSION_dup(ext)) == NULL)
		goto err;
	if (!sk_X509_EXTENSION_insert(sk, new_ext, loc))
		goto err;
	new_ext = NULL;

	*x = sk;

	return sk;

 err:
	X509_EXTENSION_free(new_ext);
	if (x != NULL && sk != *x)
		sk_X509_EXTENSION_pop_free(sk, X509_EXTENSION_free);

	return NULL;
}
LCRYPTO_ALIAS(X509v3_add_ext);

X509_EXTENSION *
X509_EXTENSION_create_by_NID(X509_EXTENSION **ext, int nid, int crit,
    ASN1_OCTET_STRING *data)
{
	const ASN1_OBJECT *obj;

	if ((obj = OBJ_nid2obj(nid)) == NULL) {
		X509error(X509_R_UNKNOWN_NID);
		return NULL;
	}

	return X509_EXTENSION_create_by_OBJ(ext, obj, crit, data);
}
LCRYPTO_ALIAS(X509_EXTENSION_create_by_NID);

X509_EXTENSION *
X509_EXTENSION_create_by_OBJ(X509_EXTENSION **ext, const ASN1_OBJECT *obj,
    int crit, ASN1_OCTET_STRING *data)
{
	X509_EXTENSION *ret;

	if (ext == NULL || (ret = *ext) == NULL)
		ret = X509_EXTENSION_new();
	if (ret == NULL) {
		X509error(ERR_R_MALLOC_FAILURE);
		goto err;
	}

	if (!X509_EXTENSION_set_object(ret, obj))
		goto err;
	if (!X509_EXTENSION_set_critical(ret, crit))
		goto err;
	if (!X509_EXTENSION_set_data(ret, data))
		goto err;

	if (ext != NULL)
		*ext = ret;

	return ret;

 err:
	if (ext == NULL || ret != *ext)
		X509_EXTENSION_free(ret);

	return NULL;
}
LCRYPTO_ALIAS(X509_EXTENSION_create_by_OBJ);

int
X509_EXTENSION_set_object(X509_EXTENSION *ext, const ASN1_OBJECT *obj)
{
	if (ext == NULL || obj == NULL)
		return 0;

	ASN1_OBJECT_free(ext->object);
	return (ext->object = OBJ_dup(obj)) != NULL;
}
LCRYPTO_ALIAS(X509_EXTENSION_set_object);

int
X509_EXTENSION_set_critical(X509_EXTENSION *ext, int crit)
{
	if (ext == NULL)
		return 0;

	ext->critical = crit ? 0xFF : -1;

	return 1;
}
LCRYPTO_ALIAS(X509_EXTENSION_set_critical);

int
X509_EXTENSION_set_data(X509_EXTENSION *ext, ASN1_OCTET_STRING *data)
{
	if (ext == NULL)
		return 0;

	return ASN1_STRING_set(ext->value, data->data, data->length);
}
LCRYPTO_ALIAS(X509_EXTENSION_set_data);

ASN1_OBJECT *
X509_EXTENSION_get_object(X509_EXTENSION *ext)
{
	if (ext == NULL)
		return NULL;

	return ext->object;
}
LCRYPTO_ALIAS(X509_EXTENSION_get_object);

ASN1_OCTET_STRING *
X509_EXTENSION_get_data(X509_EXTENSION *ext)
{
	if (ext == NULL)
		return NULL;

	return ext->value;
}
LCRYPTO_ALIAS(X509_EXTENSION_get_data);

int
X509_EXTENSION_get_critical(const X509_EXTENSION *ext)
{
	if (ext == NULL)
		return 0;
	if (ext->critical > 0)
		return 1;
	return 0;
}
LCRYPTO_ALIAS(X509_EXTENSION_get_critical);
