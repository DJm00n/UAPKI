/*
 * Copyright 2021 The UAPKI Project Authors.
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are 
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright 
 * notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright 
 * notice, this list of conditions and the following disclaimer in the 
 * documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED 
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PolicyQualifierInfo.h"

#include "asn_internal.h"

#undef FILE_MARKER
#define FILE_MARKER "pkix/struct/PolicyQualifierInfo.c"

static asn_TYPE_member_t asn_MBR_PolicyQualifierInfo_1[] = {
    {
        ATF_NOFLAGS, 0, offsetof(struct PolicyQualifierInfo, policyQualifierId),
        (ASN_TAG_CLASS_UNIVERSAL | (6 << 2)),
        0,
        &PolicyQualifierId_desc,
        0,    /* Defer constraints checking to the member type */
        0,    /* PER is not compiled, use -gen-PER */
        0,
        "policyQualifierId"
    },
    {
        ATF_OPEN_TYPE | ATF_NOFLAGS, 0, offsetof(struct PolicyQualifierInfo, qualifier),
        (ber_tlv_tag_t)-1 /* Ambiguous tag (ANY?) */,
        0,
        &ANY_desc,
        0,    /* Defer constraints checking to the member type */
        0,    /* PER is not compiled, use -gen-PER */
        0,
        "qualifier"
    },
};
static const ber_tlv_tag_t PolicyQualifierInfo_desc_tags_1[] = {
    (ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static const asn_TYPE_tag2member_t asn_MAP_PolicyQualifierInfo_tag2el_1[] = {
    { (ASN_TAG_CLASS_UNIVERSAL | (6 << 2)), 0, 0, 0 } /* policyQualifierId */
};
static asn_SEQUENCE_specifics_t asn_SPC_PolicyQualifierInfo_specs_1 = {
    sizeof(struct PolicyQualifierInfo),
    offsetof(struct PolicyQualifierInfo, _asn_ctx),
    asn_MAP_PolicyQualifierInfo_tag2el_1,
    1,    /* Count of tags in the map */
    0, 0, 0,    /* Optional elements (not needed) */
    -1,    /* Start extensions */
    -1    /* Stop extensions */
};
asn_TYPE_descriptor_t PolicyQualifierInfo_desc = {
    "PolicyQualifierInfo",
    "PolicyQualifierInfo",
    SEQUENCE_free,
    SEQUENCE_print,
    SEQUENCE_constraint,
    SEQUENCE_decode_ber,
    SEQUENCE_encode_der,
    SEQUENCE_decode_xer,
    SEQUENCE_encode_xer,
    0, 0,    /* No PER support, use "-gen-PER" to enable */
    0,    /* Use generic outmost tag fetcher */
    PolicyQualifierInfo_desc_tags_1,
    sizeof(PolicyQualifierInfo_desc_tags_1)
    / sizeof(PolicyQualifierInfo_desc_tags_1[0]), /* 1 */
    PolicyQualifierInfo_desc_tags_1,    /* Same as above */
    sizeof(PolicyQualifierInfo_desc_tags_1)
    / sizeof(PolicyQualifierInfo_desc_tags_1[0]), /* 1 */
    0,    /* No PER visible constraints */
    asn_MBR_PolicyQualifierInfo_1,
    2,    /* Elements count */
    &asn_SPC_PolicyQualifierInfo_specs_1    /* Additional specs */
};

asn_TYPE_descriptor_t *get_PolicyQualifierInfo_desc(void)
{
    return &PolicyQualifierInfo_desc;
}