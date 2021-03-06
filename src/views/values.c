/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "values.h"
#include "../bitfield.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define dec_uint16(b) (decode_raw16(*((raw_16 *) b)))
#define dec_raw24(b) (decode_raw24(*((raw_24 *) b)))

static void enc_uint16(uint16_t u, char **buf);
static void enc_raw24(uint32_t u, char **buf);


couchstore_error_t decode_view_btree_value(const char *bytes,
                                           size_t len,
                                           view_btree_value_t **value)
{
    view_btree_value_t *v = NULL;
    uint16_t  i, j;
    uint32_t  sz;
    const char    *bs;
    size_t length;

    v = (view_btree_value_t *) malloc(sizeof(view_btree_value_t));
    if (v == NULL) {
        goto alloc_error;
    }

    v->values = NULL;

    assert(len >= 2);
    v->partition = dec_uint16(bytes);
    bytes += 2;
    len -= 2;

    bs = bytes;
    length = len;

    v->num_values = 0;
    while (len > 0) {

        assert(len >= 3);
        sz = dec_raw24(bs);
        bs += 3;
        len -= 3;

        assert(len >= sz);
        bs += sz;
        len -= sz;
        v->num_values++;
    }

    if (len > 0) {
        free_view_btree_value(v);
        return COUCHSTORE_ERROR_CORRUPT;
    }

    v->values = (sized_buf *) malloc(v->num_values * sizeof(sized_buf));

    if (v->values == NULL) {
        goto alloc_error;
    }

    for (j = 0; j< v->num_values; ++j) {
        v->values[j].buf = NULL;
    }

    i = 0;
    len = length;
    while (len > 0) {

        sz = dec_raw24(bytes);
        bytes += 3;
        len -= 3;

        v->values[i].size = sz;
        v->values[i].buf = (char *) malloc(sz);

        if (v->values[i].buf == NULL) {
            goto alloc_error;
        }

        memcpy(v->values[i].buf, bytes, sz);
        bytes += sz;
        len -= sz;
        i++;
    }

    *value = v;

    return COUCHSTORE_SUCCESS;

 alloc_error:
    free_view_btree_value(v);
    return COUCHSTORE_ERROR_ALLOC_FAIL;
}


couchstore_error_t encode_view_btree_value(const view_btree_value_t *value,
                                           char **buffer,
                                           size_t *buffer_size)
{
    char *buf = NULL, *b = NULL;
    uint16_t i;
    size_t sz = 0;

    sz += 2;                 /* partition */
    /* values */
    for (i = 0; i < value->num_values; ++i) {
        sz += 3;             /* json value length */
        sz += value->values[i].size;
    }

    b = buf = (char *) malloc(sz);
    if (buf == NULL) {
        goto alloc_error;
    }

    enc_uint16(value->partition, &b);

    for (i = 0; i < value->num_values; ++i) {
        enc_raw24(value->values[i].size, &b);

        memcpy(b, value->values[i].buf, value->values[i].size);
        b += value->values[i].size;
    }

    *buffer = buf;
    *buffer_size = sz;

    return COUCHSTORE_SUCCESS;

 alloc_error:
    free(buf);
    *buffer = NULL;
    *buffer_size = 0;
    return COUCHSTORE_ERROR_ALLOC_FAIL;
}


void free_view_btree_value(view_btree_value_t *value)
{
    int i;
    if (value == NULL) {
        return;
    }

    if (value->values != NULL){
        for (i = 0; i < value->num_values; ++i) {
            free(value->values[i].buf);
        }
        free(value->values);
    }

    free(value);
}


couchstore_error_t decode_view_id_btree_value(const char *bytes,
                                              size_t len,
                                              view_id_btree_value_t **value)
{
    view_id_btree_value_t *v = NULL;
    uint16_t i, j, num_keys;
    const char *bs;
    size_t sz, length;

    v = (view_id_btree_value_t *) malloc(sizeof(view_id_btree_value_t));
    if (v == NULL) {
        goto alloc_error;
    }

    v->view_keys_map = NULL;

    assert(len >= 2);
    v->partition = dec_uint16(bytes);
    bytes += 2;
    len -= 2;

    bs = bytes;
    length = len;

    v->num_view_keys_map = 0;
    while (len > 0) {

        assert(len >= 1);
        bs += 1; /* view_id */
        len -= 1;

        assert(len >= 2);
        num_keys = dec_uint16(bs);
        bs +=2;
        len -= 2;

        for (j = 0; j < num_keys; ++j) {

            assert(len >= 2);
            sz = dec_uint16(bs);
            bs += 2;
            len -= 2;

            assert(len >= sz);
            bs += sz;
            len -= sz;
        }

        v->num_view_keys_map++;
    }

    if (len > 0) {
        free_view_id_btree_value(v);
        return COUCHSTORE_ERROR_CORRUPT;
    }

    v->view_keys_map = (view_keys_mapping_t *) malloc(v->num_view_keys_map *
                                                     sizeof(view_keys_mapping_t));

    if (v->view_keys_map == NULL) {
        goto alloc_error;
    }

    for (j = 0; j< v->num_view_keys_map; ++j) {
        v->view_keys_map[j].json_keys = NULL;
    }

    i = 0;
    len = length;
    while (len > 0) {

        v->view_keys_map[i].view_id = bytes[0];
        bytes += 1;
        len -= 1;

        num_keys = dec_uint16(bytes);
        v->view_keys_map[i].num_keys = num_keys;
        bytes += 2;
        len -= 2;

        v->view_keys_map[i].json_keys = (sized_buf *) malloc (num_keys * sizeof(sized_buf));
        if (v->view_keys_map[i].json_keys == NULL) {
            goto alloc_error;
        }

        for (j = 0; j< num_keys; ++j) {
            v->view_keys_map[i].json_keys[j].buf = NULL;
        }

        for ( j = 0; j < num_keys; ++j) {

            sz = dec_uint16(bytes);
            bytes += 2;
            len -= 2;

            v->view_keys_map[i].json_keys[j].size = sz;
            v->view_keys_map[i].json_keys[j].buf = (char *) malloc(sz);

            if (v->view_keys_map[i].json_keys[j].buf == NULL) {
                goto alloc_error;
            }

            memcpy(v->view_keys_map[i].json_keys[j].buf, bytes, sz);
            bytes += sz;
            len -= sz;
        }

        i++;
    }

    *value = v;

    return COUCHSTORE_SUCCESS;

 alloc_error:
    free_view_id_btree_value(v);
    return COUCHSTORE_ERROR_ALLOC_FAIL;
}


couchstore_error_t encode_view_id_btree_value(const view_id_btree_value_t *value,
                                              char **buffer,
                                              size_t *buffer_size)
{
    char *buf = NULL, *b = NULL;
    size_t sz = 0;
    uint16_t i, j;

    sz += 2;                 /* partition */

    /* view_keys_mappings */

    for (i = 0; i < value->num_view_keys_map; ++i) {
        sz += 1;            /*view_id */
        sz += 2;
        for (j = 0; j < value->view_keys_map[i].num_keys; ++j) {
            sz += 2;        /* size_t */
            sz += value->view_keys_map[i].json_keys[j].size;
        }
    }

    b = buf = (char *) malloc(sz);
    if (buf == NULL) {
        goto alloc_error;
    }

    enc_uint16(value->partition, &b);

    for (i = 0; i < value->num_view_keys_map; ++i) {

        b[0] = value->view_keys_map[i].view_id;
        b += 1;

        enc_uint16(value->view_keys_map[i].num_keys, &b);

        for (j = 0; j < value->view_keys_map[i].num_keys; ++j) {

            enc_uint16(value->view_keys_map[i].json_keys[j].size, &b);

            memcpy(b, value->view_keys_map[i].json_keys[j].buf,
                    value->view_keys_map[i].json_keys[j].size);
            b += value->view_keys_map[i].json_keys[j].size;

        }
    }

    *buffer = buf;
    *buffer_size = sz;

    return COUCHSTORE_SUCCESS;

 alloc_error:
    free(buf);
    *buffer = NULL;
    *buffer_size = 0;
    return COUCHSTORE_ERROR_ALLOC_FAIL;
}


void free_view_id_btree_value(view_id_btree_value_t *value)
{
    int i;
    int j;

    if (value == NULL) {
        return;
    }

    if (value->view_keys_map != NULL){
        for (i = 0; i < value->num_view_keys_map; ++i) {
            if (value->view_keys_map[i].json_keys != NULL) {
                for (j = 0; j <value->view_keys_map[i].num_keys; ++j) {
                    free (value->view_keys_map[i].json_keys[j].buf);
                }

                free(value->view_keys_map[i].json_keys);
            }
        }

        free(value->view_keys_map);
    }

    free(value);
}

static void enc_uint16(uint16_t u, char **buf)
{
    raw_16 v = encode_raw16(u);
    memcpy(*buf, &v, 2);
    *buf += 2;
}

static void enc_raw24(uint32_t u, char **buf)
{
    raw_24 v;
    encode_raw24(u, &v);
    memcpy(*buf, &v, 3);
    *buf += 3;
}
