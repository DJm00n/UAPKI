/*
 * Copyright (c) 2021, The UAPKI Project Authors.
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

#include "api-json-internal.h"
#include "cms-utils.h"
#include "doc-signflow.h"
#include "global-objects.h"
#include "http-helper.h"
#include "oid-utils.h"
#include "parson-helper.h"
#include "store-utils.h"
#include "time-utils.h"
#include "tsp-utils.h"


#undef FILE_MARKER
#define FILE_MARKER "api/sign.cpp"

#define DEBUG_OUTCON(expression)
#ifndef DEBUG_OUTCON
#define DEBUG_OUTCON(expression) expression
#endif

#define CADES_A_V3_STR "CAdES-Av3"
#define CADES_BES_STR "CAdES-BES"
#define CADES_C_STR "CAdES-C"
#define CADES_T_STR "CAdES-T"
#define CMS_STR "CMS"
#define RAW_STR "RAW"
#define SIGN_MAX_DOCS 100


using namespace  std;


static SIGNATURE_FORMAT cades_str_to_enum (const char* str_format, const SIGNATURE_FORMAT format_by_default)
{
    SIGNATURE_FORMAT rv = SIGNATURE_FORMAT::CADES_UNDEFINED;
    if (str_format == nullptr) {
        rv = format_by_default;
    }
    else if (strcmp(str_format, CADES_BES_STR) == 0) {
        rv = SIGNATURE_FORMAT::CADES_BES;
    }
    else if (strcmp(str_format, CADES_T_STR) == 0) {
        rv = SIGNATURE_FORMAT::CADES_T;
    }
    else if (strcmp(str_format, CADES_C_STR) == 0) {
        rv = SIGNATURE_FORMAT::CADES_C;
    }
    else if (strcmp(str_format, CADES_A_V3_STR) == 0) {
        rv = SIGNATURE_FORMAT::CADES_Av3;
    }
    else if (strcmp(str_format, CMS_STR) == 0) {
        rv = SIGNATURE_FORMAT::CMS_SID_KEYID;
    }
    else if (strcmp(str_format, RAW_STR) == 0) {
        rv = SIGNATURE_FORMAT::RAW;
    }
    return rv;
}

static int doc_get_docattrs (JSON_Array* jaAttrs, vector<DocAttr*>& attrs)
{
    int ret = RET_OK;
    DocAttr* doc_attr = nullptr;
    ByteArray* ba_bytes = nullptr;
    const char* s_type = nullptr;
    const size_t cnt_attrs = json_array_get_count(jaAttrs);

    for (size_t i = 0; i < cnt_attrs; i++) {
        JSON_Object* jo_attr = json_array_get_object(jaAttrs, i);
        s_type = json_object_get_string(jo_attr, "type");
        ba_bytes = json_object_get_base64(jo_attr, "bytes");
        if (!s_type || !ba_bytes) {
            return RET_UAPKI_INVALID_PARAMETER;
        }

        CHECK_NOT_NULL(doc_attr = new DocAttr(s_type, ba_bytes));
        ba_bytes = nullptr;

        attrs.push_back(doc_attr);
        doc_attr = nullptr;
    }

cleanup:
    delete doc_attr;
    ba_free(ba_bytes);
    return RET_OK;
}

static int get_info_signalgo_and_keyid (string& signAlgo, ByteArray** baKeyId)
{
    int ret = RET_OK;
    ParsonHelper json;
    CM_JSON_PCHAR cmjs_keyinfo = nullptr;
    CM_BYTEARRAY* cmba_keyid = nullptr;
    JSON_Array* ja_signalgos = nullptr;
    bool is_found = false;

    DO(CmProviders::keyGetInfo(&cmjs_keyinfo, &cmba_keyid));

    if (json.parse((const char*)cmjs_keyinfo, false) == nullptr) {
        SET_ERROR(RET_UAPKI_INVALID_JSON_FORMAT);
    }

    ja_signalgos = json.getArray("signAlgo");
    if (json_array_get_count(ja_signalgos) == 0) {
        SET_ERROR(RET_UAPKI_UNSUPPORTED_ALG);
    }

    if (signAlgo.empty()) {
        //  Set first signAlgo from list
        signAlgo = ParsonHelper::jsonArrayGetString(ja_signalgos, 0);
        is_found = (!signAlgo.empty());
    }
    else {
        //  Check signAlgo in list
        for (size_t i = 0; i < json_array_get_count(ja_signalgos); i++) {
            const string s = ParsonHelper::jsonArrayGetString(ja_signalgos, i);
            is_found = (s == signAlgo);
            if (is_found) break;
        }
    }
    if (!is_found) {
        SET_ERROR(RET_UAPKI_UNSUPPORTED_ALG);
    }

    *baKeyId = ba_alloc_from_uint8(cmba_keyid->buf, cmba_keyid->len);
    if (*baKeyId == nullptr) {
        SET_ERROR(RET_UAPKI_GENERAL_ERROR);
    }

cleanup:
    CmProviders::free(cmjs_keyinfo);
    CmProviders::baFree(cmba_keyid);
    return ret;
}

static int parse_sign_params (JSON_Object* joSignParams, SignParams& signParams)
{
    int ret = RET_OK;

    signParams.signatureFormat = cades_str_to_enum(json_object_get_string(joSignParams, "signatureFormat"), SIGNATURE_FORMAT::CADES_BES);
    signParams.signAlgo = ParsonHelper::jsonObjectGetString(joSignParams, "signAlgo");
    signParams.digestAlgo = ParsonHelper::jsonObjectGetString(joSignParams, "digestAlgo");
    signParams.detachedData = ParsonHelper::jsonObjectGetBoolean(joSignParams, "detachedData", true);
    signParams.includeCert = ParsonHelper::jsonObjectGetBoolean(joSignParams, "includeCert", false);
    signParams.includeTime = ParsonHelper::jsonObjectGetBoolean(joSignParams, "includeTime", false);
    signParams.includeContentTS = ParsonHelper::jsonObjectGetBoolean(joSignParams, "includeContentTS", false);

    switch (signParams.signatureFormat) {
    case SIGNATURE_FORMAT::CADES_T:
        signParams.includeContentTS = true;
        signParams.includeSignatureTS = true;
    case SIGNATURE_FORMAT::CADES_BES:
        signParams.sidUseKeyId = false;
        break;
    case SIGNATURE_FORMAT::CMS_SID_KEYID:
        signParams.sidUseKeyId = true;
        break;
    case SIGNATURE_FORMAT::RAW:
        break;
    case SIGNATURE_FORMAT::CADES_Av3:
    case SIGNATURE_FORMAT::CADES_C:
    default:
        ret = RET_UAPKI_INVALID_PARAMETER;
    }

    return ret;
}

static int parse_sigpolicy_and_encode_attrvalue (JSON_Object* joSignPolicyParams, ByteArray** baEncoded)
{
    if (!joSignPolicyParams) return RET_OK;

    const string sig_policyid = ParsonHelper::jsonObjectGetString(joSignPolicyParams, "sigPolicyId");
    if (sig_policyid.empty()) return RET_UAPKI_INVALID_PARAMETER;

    int ret = RET_OK;
    SignaturePolicyIdentifier_t* spi = nullptr;

    ASN_ALLOC_TYPE(spi, SignaturePolicyIdentifier_t);

    spi->present = SignaturePolicyIdentifier_PR_signaturePolicyId;
    DO(asn_set_oid_from_text(sig_policyid.c_str(), &spi->choice.signaturePolicyId.sigPolicyId));

    DO(asn_encode_ba(get_SignaturePolicyIdentifier_desc(), spi, baEncoded));

cleanup:
    asn_free(get_SignaturePolicyIdentifier_desc(), spi);
    return ret;
}

static int tsp_process (MessageImprintParams& msgimParams, TspRequestParams& tspParams, ByteArray** baTsToken)
{
    int ret = RET_OK;
    const LibraryConfig::TspParams& tsp_config = get_config()->getTsp();
    ByteArray* ba_req = nullptr;
    ByteArray* ba_resp = nullptr;
    ByteArray* ba_tstinfo = nullptr;
    ByteArray* ba_tstoken = nullptr;
    uint8_t byte = 0;
    uint32_t status = 0;

    CHECK_PARAM(baTsToken != nullptr);

    if (tspParams.nonce != nullptr) {
        DO(drbg_random(tspParams.nonce));
        DO(ba_get_byte(tspParams.nonce, 0, &byte));
        DO(ba_set_byte(tspParams.nonce, 0, byte & 0x7F));  //  Integer must be a positive number
    }

    DO(tsp_request_encode(&msgimParams, &tspParams, &ba_req));
    DEBUG_OUTCON(printf("tsp_process(), request: "); ba_print(stdout, ba_req));

    ret = HttpHelper::post(tsp_config.url.c_str(), HttpHelper::CONTENT_TYPE_TSP_REQUEST, ba_req, &ba_resp);
    if (ret != RET_OK) {
        SET_ERROR(RET_UAPKI_TSP_NOT_RESPONDING);
    }

    DEBUG_OUTCON(printf("tsp_process(), response: "); ba_print(stdout, ba_resp));
    DO(tsp_response_parse(ba_resp, &status, &ba_tstoken, &ba_tstinfo));
    if ((status != PKIStatus_granted) && (status != PKIStatus_grantedWithMods)) {
        //ret = parse_tsp_response_statusinfo(ba_tsr, &status, &s_statusString, &failInfo);
        SET_ERROR(RET_UAPKI_TSP_RESPONSE_NOT_GRANTED);
    }

    DO(tsp_tstinfo_is_equal_request(ba_req, ba_tstinfo));

    *baTsToken = ba_tstoken;
    ba_tstoken = nullptr;

cleanup:
    ba_free(ba_req);
    ba_free(ba_resp);
    ba_free(ba_tstinfo);
    ba_free(ba_tstoken);
    return ret;
}

static int docattr_add (const char* type, const ByteArray* baEncoded, vector<DocAttr*>& attrs)
{
    int ret = RET_OK;
    DocAttr* doc_attr = nullptr;
    ByteArray* ba_value = nullptr;

    CHECK_NOT_NULL(ba_value = ba_copy_with_alloc(baEncoded, 0, 0));

    CHECK_NOT_NULL(doc_attr = new DocAttr(type, ba_value));
    ba_value = nullptr;

    attrs.push_back(doc_attr);
    doc_attr = nullptr;

cleanup:
    delete doc_attr;
    ba_free(ba_value);
    return ret;
}

static int sattr_add_content_ts (SigningDoc& sdoc)
{
    int ret = RET_OK;
    MessageImprintParams msgim_params;
    TspRequestParams tsp_params;
    ByteArray* ba_tstoken = nullptr;

    msgim_params.hashAlgo = sdoc.signParams->digestAlgo.c_str();
    msgim_params.hashAlgoParam_isNULL = false;
    msgim_params.hashedMessage = sdoc.baMessageDigest;
    CHECK_NOT_NULL(tsp_params.nonce = ba_alloc_by_len(8));
    tsp_params.certReq = false;
    tsp_params.reqPolicy = nullptr;

    DO(tsp_process(msgim_params, tsp_params, &ba_tstoken));

    docattr_add(OID_PKCS9_CONTENT_TIMESTAMP, ba_tstoken, sdoc.signedAttrs);
    ba_tstoken = nullptr;

cleanup:
    ba_free(ba_tstoken);
    return ret;
}

static int unsattr_add_signature_ts (SigningDoc& sdoc)
{
    int ret = RET_OK;
    MessageImprintParams msgim_params;
    TspRequestParams tsp_params;
    ByteArray* ba_hash = nullptr;
    ByteArray* ba_tstoken = nullptr;

    DO(sdoc.digestSignature(&ba_hash));

    msgim_params.hashAlgo = sdoc.signParams->digestAlgo.c_str();
    msgim_params.hashAlgoParam_isNULL = false;//reserved, get from settings
    msgim_params.hashedMessage = ba_hash;
    CHECK_NOT_NULL(tsp_params.nonce = ba_alloc_by_len(8));//now always nonce, get from settings
    tsp_params.certReq = false;//reserved, get from settings
    tsp_params.reqPolicy = nullptr;//reserved, get from settings

    DO(tsp_process(msgim_params, tsp_params, &ba_tstoken));

    docattr_add(OID_PKCS9_TIMESTAMP_TOKEN, ba_tstoken, sdoc.unsignedAttrs);
    ba_tstoken = nullptr;

cleanup:
    ba_free(ba_hash);
    ba_free(ba_tstoken);
    return ret;
}


int uapki_sign (JSON_Object* joParams, JSON_Object* joResult)
{
    int ret = RET_OK;
    LibraryConfig* config = nullptr;
    CerStore* cer_store = nullptr;
    SignParams sign_params;
    size_t cnt_docs = 0;
    JSON_Array* ja_results = nullptr;
    JSON_Array* ja_sources = nullptr;
    vector<SigningDoc> signing_docs;
    vector<const CM_BYTEARRAY*> refcmba_hashes;
    CM_BYTEARRAY** cmba_signatures = nullptr;

    config = get_config();
    cer_store = get_cerstore();
    if (!config || !cer_store) {
        SET_ERROR(RET_UAPKI_GENERAL_ERROR);
    }

    ret = CmProviders::keyIsSelected();
    if (ret != RET_OK) return ret;

    DO(parse_sign_params(json_object_get_object(joParams, "signParams"), sign_params));
    DO(parse_sigpolicy_and_encode_attrvalue(json_object_dotget_object(joParams, "signParams.signaturePolicy"), &sign_params.baSignPolicy));

    if (sign_params.includeContentTS || sign_params.includeSignatureTS) {
        if (config->getOffline()) {
            SET_ERROR(RET_UAPKI_OFFLINE_MODE);
        }
        DEBUG_OUTCON(printf("config->getTsp().url: '%s'\n", config->getTsp().url.c_str()));
        if (config->getTsp().url.empty()) {
            SET_ERROR(RET_UAPKI_TSP_URL_NOT_PRESENT);
        }
    }

    ja_sources = json_object_get_array(joParams, "dataTbs");
    cnt_docs = json_array_get_count(ja_sources);
    if ((cnt_docs == 0) || (cnt_docs > SIGN_MAX_DOCS)) { 
        SET_ERROR(RET_UAPKI_INVALID_PARAMETER);
    }

    DO(get_info_signalgo_and_keyid(sign_params.signAlgo, &sign_params.baKeyId));
    sign_params.signHashAlgo = hash_from_oid(sign_params.signAlgo.c_str());
    if (sign_params.signHashAlgo == HashAlg::HASH_ALG_UNDEFINED) {
        SET_ERROR(RET_UAPKI_UNSUPPORTED_ALG);
    }

    if (sign_params.digestAlgo.empty() || (sign_params.signatureFormat == SIGNATURE_FORMAT::RAW)) {
        sign_params.digestHashAlgo = sign_params.signHashAlgo;
        sign_params.digestAlgo = string(hash_to_oid(sign_params.digestHashAlgo));
    }
    else {
        sign_params.digestHashAlgo = hash_from_oid(sign_params.digestAlgo.c_str());
    }
    if (sign_params.digestHashAlgo == HashAlg::HASH_ALG_UNDEFINED) {
        SET_ERROR(RET_UAPKI_UNSUPPORTED_ALG);
    }

    if ((sign_params.signatureFormat != SIGNATURE_FORMAT::RAW) && ((!sign_params.sidUseKeyId || sign_params.includeCert))) {
        DO(cer_store->getCertByKeyId(sign_params.baKeyId, &sign_params.cerStoreItem));
    }

    if ((sign_params.signatureFormat >= SIGNATURE_FORMAT::CADES_BES) && (sign_params.signatureFormat <= SIGNATURE_FORMAT::CADES_Av3)) {
        DO(gen_attrvalue_ess_certid_v2(sign_params.digestHashAlgo, sign_params.cerStoreItem->baEncoded, &sign_params.baEssCertId));
    }

    signing_docs.resize(cnt_docs);
    //  Parse and load all TBS-data
    for (size_t i = 0; i < signing_docs.size(); i++) {
        SigningDoc& sdoc = signing_docs[i];
        JSON_Object* jo_doc = json_array_get_object(ja_sources, i);

        DO(sdoc.init(&sign_params, json_object_get_string(jo_doc, "id"), json_object_get_base64(jo_doc, "bytes")));
        sdoc.isDigest = ParsonHelper::jsonObjectGetBoolean(jo_doc, "isDigest", false);
        if (sign_params.signatureFormat != SIGNATURE_FORMAT::RAW) {
            DO(doc_get_docattrs(json_object_get_array(jo_doc, "signedAttributes"), sdoc.signedAttrs));
            DO(doc_get_docattrs(json_object_get_array(jo_doc, "unsignedAttributes"), sdoc.unsignedAttrs));
        }
    }

    if (sign_params.signatureFormat != SIGNATURE_FORMAT::RAW) {
        for (size_t i = 0; i < signing_docs.size(); i++) {
            SigningDoc& sdoc = signing_docs[i];

            if (sign_params.baEssCertId) {
                DO(docattr_add(OID_PKCS9_SIGNING_CERTIFICATE_V2, sign_params.baEssCertId, sdoc.signedAttrs));
            }
            if (sign_params.baSignPolicy) {
                DO(docattr_add(OID_PKCS9_SIG_POLICY_ID, sign_params.baSignPolicy, sdoc.signedAttrs));
            }

            DO(sdoc.digestMessage());

            //  Add signed-attribute before call buildSignedAttributes
            if (sign_params.includeContentTS) {
                DO(sattr_add_content_ts(sdoc));
            }

            DO(sdoc.buildSignedAttributes());
            DO(sdoc.digestSignedAttributes());
            refcmba_hashes.push_back((const CM_BYTEARRAY*)sdoc.baHashSignedAttrs);
        }

        DO(CmProviders::keySign((const CM_UTF8_CHAR*)sign_params.signAlgo.c_str(), nullptr, (uint32_t)refcmba_hashes.size(),
            (const CM_BYTEARRAY**)refcmba_hashes.data(), &cmba_signatures));

        for (size_t i = 0; i < signing_docs.size(); i++) {
            SigningDoc& sdoc = signing_docs[i];
            signing_docs[i].baSignature = ba_copy_with_alloc((const ByteArray*)cmba_signatures[i], 0, 0);
            //  Add unsigned attrs before call buildSignedData
            if (sign_params.includeSignatureTS) {
                DO(unsattr_add_signature_ts(sdoc));
            }
            DO(sdoc.buildUnsignedAttributes());
            DO(sdoc.buildSignedData());
        }
    }
    else {
        for (size_t i = 0; i < signing_docs.size(); i++) {
            SigningDoc& sdoc = signing_docs[i];
            DO(sdoc.digestMessage());
            refcmba_hashes.push_back((const CM_BYTEARRAY*)sdoc.baMessageDigest);
        }

        DO(CmProviders::keySign((const CM_UTF8_CHAR*)sign_params.signAlgo.c_str(), nullptr, (uint32_t)refcmba_hashes.size(),
            (const CM_BYTEARRAY**)refcmba_hashes.data(), &cmba_signatures));

        for (size_t i = 0; i < signing_docs.size(); i++) {
            signing_docs[i].baEncoded = ba_copy_with_alloc((const ByteArray*)cmba_signatures[i], 0, 0);
        }
    }

    DO_JSON(json_object_set_value(joResult, "signatures", json_value_init_array()));
    ja_results = json_object_get_array(joResult, "signatures");
    for (size_t i = 0; i < signing_docs.size(); i++) {
        JSON_Object* jo_doc = nullptr;
        SigningDoc& sdoc = signing_docs[i];
        DO_JSON(json_array_append_value(ja_results, json_value_init_object()));
        if ((jo_doc = json_array_get_object(ja_results, i)) == nullptr) {
            SET_ERROR(RET_UAPKI_GENERAL_ERROR);
        }
        DO_JSON(json_object_set_string(jo_doc, "id", sdoc.id));
        DO_JSON(json_object_set_base64(jo_doc, "bytes", sdoc.baEncoded));
    }

cleanup:
    if (cmba_signatures != nullptr) {
        CmProviders::arrayBaFree((uint32_t)refcmba_hashes.size(), cmba_signatures);
    }
    return ret;
}
