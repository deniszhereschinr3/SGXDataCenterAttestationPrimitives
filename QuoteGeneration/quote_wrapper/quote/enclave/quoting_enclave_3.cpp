/*
 * Copyright (C) 2011-2021 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
 /**
  * File: quoting_enclave.cpp
  *
  * Description: The implementation of the
  * reference ECDSA Quoting enclave
  * interfaces.
  *
  */

#include <sgx_secure_align.h>
#include <sgx_random_buffers.h>
#include <string.h>

#include "sgx_quote.h"
#include "sgx_quote_3.h"
#include "sgx_tseal.h"
#include "sgx_utils.h"
#include "qe3_t.c"
#include "sgx_tcrypto.h"
#include "sgx_trts.h"

#include "qe3.h"
#include "user_types.h"
#include "sgx_pce.h"
#include "sgx_lfence.h"

  //Compile switches for debug and for currently unsupported features.
  //#define ALLOW_DEBUG_PCE 1
  //#define ALLOW_CLEARTEXT_PPID 1
  //#define USE_ALPHA_ENCRYPTION_KEY 1
  //#define GENERATE_RANDOM_ATTESTATION_KEY 1

#define REF_N_SIZE_IN_BYTES    384
#define REF_E_SIZE_IN_BYTES    4
#define REF_D_SIZE_IN_BYTES    384
#define REF_P_SIZE_IN_BYTES    192
#define REF_Q_SIZE_IN_BYTES    192
#define REF_DMP1_SIZE_IN_BYTES 192
#define REF_DMQ1_SIZE_IN_BYTES 192
#define REF_IQMP_SIZE_IN_BYTES 192

#define REF_N_SIZE_IN_UINT     REF_N_SIZE_IN_BYTES/sizeof(unsigned int)
#define REF_E_SIZE_IN_UINT     REF_E_SIZE_IN_BYTES/sizeof(unsigned int)
#define REF_D_SIZE_IN_UINT     REF_D_SIZE_IN_BYTES/sizeof(unsigned int)
#define REF_P_SIZE_IN_UINT     REF_P_SIZE_IN_BYTES/sizeof(unsigned int)
#define REF_Q_SIZE_IN_UINT     REF_Q_SIZE_IN_BYTES/sizeof(unsigned int)
#define REF_DMP1_SIZE_IN_UINT  REF_DMP1_SIZE_IN_BYTES/sizeof(unsigned int)
#define REF_DMQ1_SIZE_IN_UINT  REF_DMQ1_SIZE_IN_BYTES/sizeof(unsigned int)
#define REF_IQMP_SIZE_IN_UINT  REF_IQMP_SIZE_IN_BYTES/sizeof(unsigned int)

#ifdef ALLOW_CLEARTEXT_PPID
typedef struct _ref_rsa_params_t {
    unsigned int n[REF_N_SIZE_IN_UINT];
    unsigned int e[REF_E_SIZE_IN_UINT];
    unsigned int d[REF_D_SIZE_IN_UINT];
    unsigned int p[REF_P_SIZE_IN_UINT];
    unsigned int q[REF_Q_SIZE_IN_UINT];
    unsigned int dmp1[REF_DMP1_SIZE_IN_UINT];
    unsigned int dmq1[REF_DMQ1_SIZE_IN_UINT];
    unsigned int iqmp[REF_IQMP_SIZE_IN_UINT];
}ref_rsa_params_t;

static ref_rsa_params_t g_rsa_key = { 0 };  // The private key used to encrypt the PPID.  Only used for PPID_CEARTEXT Cert_Data_Type
#endif
static const uint8_t g_vendor_id[16] =
{ 0x93,0x9A,0x72,0x33,0xF7,0x9C,0x4C,0xA9,0x94,0x0A,0x0D,0xB3,0x95,0x7F,0x06,0x07 };

static const uint8_t g_ref_pubkey_e_be[REF_E_SIZE_IN_BYTES] = { 0x00,0x01,0x00,0x01 };
#ifdef USE_ALPHA_ENCRYPTION_KEY
static const uint8_t g_ref_pubkey_n_be[REF_N_SIZE_IN_BYTES] =
{ 0xbb,0x01,0x00,0x8a,0x64,0x1b,0xa8,0xcb,0xbe,0x4a,0x7c,0x8b,0x6e,0x77,0x5d,0xc0,
 0x88,0xca,0x64,0x80,0x26,0x88,0x40,0xf6,0xce,0x06,0x3e,0xef,0xce,0x91,0xda,0xd3,
 0x70,0x1b,0xd4,0xbb,0xa4,0xca,0x55,0x42,0x8c,0x1b,0x21,0xc0,0x0f,0x1b,0x7e,0x70,
 0xc3,0xe4,0x7a,0x1c,0x16,0x1e,0x11,0x4c,0xd7,0x39,0xe4,0xc1,0x0d,0xbd,0x0c,0xb9,
 0x52,0x0c,0x72,0xa7,0xbd,0x3d,0x7f,0xe4,0x87,0xd6,0x51,0x67,0x63,0x70,0x90,0xde,
 0xf2,0xb1,0xf2,0xb6,0x74,0xcf,0xb6,0x9e,0xa7,0x85,0x32,0xef,0x5a,0x7c,0x6b,0x5b,
 0xf3,0x73,0x7a,0x41,0x97,0xa6,0x82,0x00,0x00,0xe5,0x70,0xd3,0x5d,0x57,0xee,0x30,
 0xe5,0x71,0x81,0x42,0x55,0xc1,0x71,0x5e,0x0d,0xce,0xb2,0x61,0xb3,0xac,0x77,0x6d,
 0xce,0xfd,0x09,0x3b,0x91,0xd1,0xcb,0x9a,0x6a,0xe4,0x99,0xee,0xfd,0xdd,0x43,0xeb,
 0xf0,0x97,0xf6,0xe8,0xd1,0x6f,0x5c,0x5b,0xb3,0x9d,0x93,0x2a,0x7b,0xcb,0x21,0xe7,
 0x87,0xf1,0x6f,0x3b,0x11,0x26,0x22,0xe2,0x36,0xcf,0x22,0x2e,0x2f,0x1f,0x50,0x0a,
 0x9f,0x97,0xd0,0xc8,0x60,0x0a,0x69,0x65,0x6b,0x0e,0xed,0xc4,0x78,0x14,0xcc,0x3a,
 0xd1,0x02,0x7d,0x37,0xa1,0xc1,0x45,0x54,0xd8,0xcc,0x34,0x11,0x09,0xc3,0xb5,0x19,
 0xa7,0x19,0x56,0xa6,0xee,0xa7,0x41,0xc5,0x50,0xf9,0x1f,0x58,0x7e,0x60,0xfa,0xa3,
 0x7a,0xe8,0x3c,0x7e,0x61,0x2b,0xcd,0x51,0x24,0x43,0xb7,0x9a,0x2d,0xeb,0x12,0x3b,
 0xfa,0xdf,0x25,0x29,0xf1,0xfa,0xbc,0x82,0xb5,0x86,0x4d,0x93,0x83,0x03,0xe9,0x76,
 0xb6,0x63,0x26,0x29,0x1e,0x03,0xc2,0x5e,0x23,0x25,0x7a,0x0b,0x00,0x37,0x87,0xfc,
 0xe7,0x18,0x85,0xbc,0xe5,0x8c,0x62,0xe6,0x39,0x6c,0xd5,0x47,0x81,0x3e,0xee,0xb3,
 0x08,0x6e,0x15,0x81,0xb3,0x47,0xf8,0xbd,0x6b,0x8e,0x32,0xf3,0x6f,0xcf,0x6b,0xe9,
 0x38,0x5c,0xfd,0xa8,0xef,0xce,0x84,0x98,0xd8,0x05,0xfb,0xa4,0xb1,0x77,0x9f,0x04,
 0x61,0x3d,0x84,0x9e,0x4d,0x4f,0xa4,0x31,0xe0,0xa3,0xb8,0x26,0xcb,0x87,0x0c,0x29,
 0x1f,0x71,0x5c,0x0d,0xe4,0xb6,0x18,0xd2,0x41,0xd7,0xb1,0xd4,0x33,0x32,0x9e,0xf4,
 0x03,0x19,0xd4,0x5b,0x95,0x4f,0xd1,0x5c,0x71,0x87,0x79,0xbc,0xf5,0x32,0x89,0x07,
 0x1d,0xf4,0x67,0x42,0xf3,0xe3,0xec,0x4a,0xd8,0xb4,0x9e,0x88,0xdf,0xbd,0x19,0xbf };
#else
static const uint8_t g_ref_pubkey_n_be[REF_N_SIZE_IN_BYTES] =
{ 0xd3,0x96,0xf9,0x43,0x43,0x11,0x00,0x1c,0x69,0x44,0x9c,0x3b,0xfd,0xee,0x8f,0x38,
0xcd,0x95,0xcd,0xad,0x74,0x09,0x7c,0x87,0xf1,0xa7,0x65,0x02,0x4c,0x87,0xc1,0x57,
0x30,0xa5,0xc9,0xa6,0xa4,0xcc,0xf9,0x1d,0x62,0x18,0x1e,0x00,0xa6,0x74,0x27,0x58,
0x59,0xca,0x1b,0x1d,0xf5,0x31,0x0e,0xf2,0xd5,0xe1,0x79,0x37,0x39,0x94,0x3d,0x3d,
0xe2,0x50,0x93,0x12,0xd6,0x03,0xe5,0x19,0x3a,0x48,0xf0,0xae,0x0c,0x37,0xee,0xe0,
0x57,0x27,0xbd,0xec,0x17,0x1b,0x0f,0x39,0x86,0x06,0x54,0x20,0x74,0x84,0x34,0xbe,
0x34,0xfa,0x71,0x6f,0xa1,0xf5,0x4c,0x9a,0x52,0x0f,0xc4,0xbc,0x2d,0x7a,0x2e,0x17,
0xe3,0x5d,0xa2,0x0e,0xca,0x39,0x07,0x98,0xa9,0x05,0x1a,0x34,0xfb,0x8f,0x60,0x9c,
0x3a,0x1e,0x26,0x30,0x0b,0xf3,0xf3,0x49,0x40,0xd9,0xf7,0x5d,0xcb,0xd1,0xbf,0x57,
0x8d,0xe5,0x2d,0xce,0x98,0x57,0x35,0xf1,0x93,0xc3,0x19,0x2e,0x80,0x55,0x37,0xab,
0x8d,0x64,0x08,0xda,0xe6,0xdd,0x64,0xb4,0x62,0x83,0x8d,0x43,0xaa,0xd2,0x7b,0xc2,
0x63,0xaa,0x97,0xde,0xed,0x09,0x92,0xd6,0x88,0x56,0x86,0xcd,0x08,0x23,0x03,0x27,
0x9a,0x78,0x7c,0xf4,0x36,0x12,0xf5,0xb1,0xe6,0x1d,0x54,0xab,0x88,0x69,0xff,0x18,
0x4f,0xdc,0x87,0xee,0x34,0xa6,0x68,0xb1,0x81,0x67,0xb6,0xce,0x0a,0x70,0x14,0xbc,
0xb3,0xe1,0x8d,0x76,0x1c,0x73,0xde,0x00,0xab,0x41,0xca,0x40,0x51,0x53,0x63,0x04,
0xc3,0x63,0x0b,0xca,0x62,0xda,0xaa,0x9c,0xe5,0x01,0xb7,0xc0,0x0f,0x7e,0x0b,0xb0,
0xbe,0xe9,0xf8,0x0d,0xb3,0xb6,0x64,0xfd,0xcd,0x95,0x17,0x9c,0x57,0x8e,0xec,0xc4,
0xac,0x8b,0x36,0x01,0x5e,0x4c,0x6d,0x1e,0x21,0x49,0xa0,0x1d,0xde,0x04,0x39,0x6b,
0x34,0x68,0x44,0xea,0x06,0x76,0xe0,0x8d,0x1f,0xa2,0xc0,0x26,0x05,0xcc,0x91,0xbe,
0xa3,0x17,0xc8,0x75,0x46,0x85,0x10,0x39,0x16,0x50,0x8e,0x02,0x43,0x98,0x31,0x70,
0x69,0xd8,0x34,0x71,0x82,0xe7,0x48,0x26,0xcd,0xc1,0x82,0xd3,0xeb,0x6f,0xe9,0x58,
0xe7,0x06,0x77,0x10,0x1f,0xdf,0x49,0x76,0x30,0xa7,0x68,0x42,0xb0,0x16,0xd7,0xda,
0x92,0x75,0xd5,0x7f,0x2e,0x75,0x43,0xac,0x83,0xb0,0x1f,0xc3,0x90,0x19,0xce,0xaa,
0x94,0xd0,0x2e,0x5a,0x6c,0x13,0x72,0xe7,0xa6,0xb5,0xc0,0x45,0x81,0xe3,0x53,0x27 };
#endif

/* One field in sgx_quote3_t(signature_len) is not part of the quote_body
   need to be signed by EPID. So we need to minus sizeof(uint32_t). */
#define QE_QUOTE_BODY_SIZE  (sizeof(sgx_quote3_t) - sizeof(uint32_t))

const uint32_t g_sgx_nistp256_r_m1[] = {//hard-coded value for n-1 where n is order of the ECC group used
    0xFC632550, 0xF3B9CAC2, 0xA7179E84, 0xBCE6FAAD, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF };

#define HASH_DRBG_OUT_LEN 40 //320 bits
static const char QE_ID_STRING[] = "QE_ID_DER";
static const char QE_ATT_STRING[] = "QE_ATT_DER";

#define MAX_CERT_DATA_SIZE (4098*3)
#define MIN_CERT_DATA_SIZE (500)

#ifdef ENABLE_QE3_LOGGING
/*
 * printf:
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
void printf(const char *fmt, ...)
{
    char buf[BUFSIZ] = { '\0' };
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    qe_ocall_print_string(buf);
}
// Some utility functions to output some of the data structures.
#define ENC_PRINT_BYTE_ARRAY(mem, len)               \
{                                                      \
    if (!mem || !len)                                  \
    {                                                  \
        printf("\n( null )\n");                 \
    }                                                  \
    else                                               \
    {                                                  \
        uint8_t *array = (uint8_t *)mem;               \
        uint32_t i = 0;                                \
        for (i = 0; i < len - 1; i++)                  \
        {                                              \
            printf("%02x", array[i]);           \
            if (i % 32 == 31) printf("\n");     \
        }                                              \
        printf("%02x", array[i]);               \
    }                                                  \
}
#endif

/**
 * Generates the attestation key based on the QE3's seal key at the current raw TCB.  The attestaion key will change
 * when the platform TCB (CPUSVN and QE3 ISVSVN) changes. The attestation key can be 'refreshed' for key hygeine by
 * inputting a differnt key_id.
 * (///todo:  The current wrappers don't allow modification of key_id).
 *
 * Derivation:
 * 1) Sealing Key = EGETKEY(KEYNAME = SEAL_KEY,
 *                            KEY_POLICY =MRSIGNER,
 *                            KEY_ID = 0, Current CPUSVN,
 *                            Current ISVSVN)
 * 2) Block 1 = AES-CMAC(Sealing Key, QE ATT string with Counter = 0x01)
 * 3) Block 2 = AES-CMAC(Sealing Key, QE ATT string with Counter = 0x02)
 * 4) Block 3 = AES-CMAC(Sealing Key, QE ATT string with Counter = 0x03)
 * 5) QE3 ATT Seed = most significant 320 bits of (Block 1 || Block 2 || Block 3).
 * 6) QE3 ATT key pair ir is generated d using NIST SP 186-4 4 section B 4.1 "Key Pair Generation Using Extra Random
 *    Bits." AE ATT Seed are used for the random bits.
 *
 *    QE ATT String:
 *       Byte Position  |  Value
 *         0            |  Counter (See Description)
 *         1-10         |  "QE_KEY_DER" (ascii encoded)
 *         11-13        |  0x000000
 *         14-15        |  0x0140 (Big Endian)
 *
 * @param p_att_priv_key[Out] Pointer to the returned value of tha attestion key private key.  Must not be NULL.
 * @param p_att_pub_key[Out] Pointer to the returned value of tha attestion key public key.  Must not be NULL.
 * @param p_req_key_id[In] Pointer to the key_id to use when generating the attestation key.
 *
 * @return REFQE3_SUCCESS Successfully created the key and the private and public keys are returned.
 * @return REFQE3_ERROR_INVALID_PARAMETER  One of the inputted parameter is NULL
 * @return REFQE3_ERROR_OUT_OF_MEMORY Heap memory was exhausted.
 * @return REFQE3_ERROR_CRYPTO Error in the crypto library functions ues to generate the key.
 * @return REFQE3_ERROR_UNEXPECTED Unexpected internal error.
 */
static qe3_error_t get_att_key_based_from_seal_key(sgx_ec256_private_t *p_att_priv_key,
    sgx_ec256_public_t  *p_att_pub_key,
    const sgx_key_id_t *p_req_key_id)
{

    sgx_status_t sgx_status = SGX_SUCCESS;

    sgx_key_request_t att_priv_key_seed_req;
    //sgx_key_128bit_t key_tmp;
    //
    // securely align seed
    //
    sgx::custom_alignment_aligned<sgx_key_128bit_t, sizeof(sgx_key_128bit_t), 0, sizeof(sgx_key_128bit_t)> okey_tmp;
    sgx_key_128bit_t* pkey_tmp = &okey_tmp.v;

    uint8_t content[16];
    sgx_cmac_128bit_tag_t block;
    sgx_report_t qe3_report;
    uint8_t hash_drg_output[HASH_DRBG_OUT_LEN];
    uint32_t i;
    qe3_error_t ret = REFQE3_ERROR_CRYPTO;

    // Defense-in-depth.  This function is only called internally so should never be NULL
    if ((NULL == p_att_priv_key) ||
        (NULL == p_att_pub_key) ||
        (NULL == p_req_key_id)) {
        return REFQE3_ERROR_INVALID_PARAMETER;
    }

    memset(&content, 0, sizeof(content));
    memset(&block, 0, sizeof(block));
    memset(pkey_tmp, 0, sizeof(*pkey_tmp));
    //1-10bytes: "QE_ATT_DER"(ascii encoded)
    memcpy(content + 1, QE_ATT_STRING, 10);
    //14-15bytes: 0x0140 (Big Endian)
    content[14] = 0x01;
    content[15] = 0x40;

    // Get PSVN from self report
    sgx_status = sgx_create_report(NULL, NULL, &qe3_report);
    if (SGX_SUCCESS != sgx_status) {
        if (SGX_ERROR_OUT_OF_MEMORY == sgx_status) {
            ret = REFQE3_ERROR_OUT_OF_MEMORY;
        }
        else {
            ret = REFQE3_ERROR_UNEXPECTED;
        }
        goto ret_point;
    }

    // Set up the key request structure.
    memset(&att_priv_key_seed_req, 0, sizeof(sgx_key_request_t));
    memcpy(&att_priv_key_seed_req.cpu_svn, &qe3_report.body.cpu_svn, sizeof(att_priv_key_seed_req.cpu_svn));
    memcpy(&att_priv_key_seed_req.isv_svn, &qe3_report.body.isv_svn, sizeof(att_priv_key_seed_req.isv_svn));
    memcpy(&att_priv_key_seed_req.key_id, p_req_key_id, sizeof(att_priv_key_seed_req.key_id));
    att_priv_key_seed_req.key_name = SGX_KEYSELECT_SEAL; // Seal key
    att_priv_key_seed_req.key_policy = SGX_KEYPOLICY_MRSIGNER;
    att_priv_key_seed_req.attribute_mask.xfrm = 0;
    att_priv_key_seed_req.misc_mask = 0xFFFFFFFF;
    att_priv_key_seed_req.attribute_mask.flags = ~SGX_FLAGS_MODE64BIT; //set all bits except the SGX_FLAGS_MODE64BIT
    sgx_status = sgx_get_key(&att_priv_key_seed_req, pkey_tmp);
    if (SGX_SUCCESS != sgx_status)
    {
        (void)memset_s(pkey_tmp, sizeof(*pkey_tmp), 0, sizeof(*pkey_tmp));
        ret = REFQE3_ERROR_CRYPTO;
        goto ret_point;
    }

    ref_static_assert(sizeof(sgx_cmac_128bit_key_t) == sizeof(sgx_key_128bit_t));
    ref_static_assert(2 * sizeof(sgx_cmac_128bit_tag_t) <= HASH_DRBG_OUT_LEN && 3 * sizeof(sgx_cmac_128bit_tag_t) >= HASH_DRBG_OUT_LEN);

    //Block 1 = AES-CMAC(Seal Key, QE ATT string with Counter = 0x01)
    content[0] = 0x01;
    if ((sgx_status = sgx_rijndael128_cmac_msg(reinterpret_cast<const sgx_cmac_128bit_key_t *>(*pkey_tmp),
        content,
        sizeof(content),
        &block)) != SGX_SUCCESS) {
        if (sgx_status == SGX_ERROR_OUT_OF_MEMORY) {
            ret = REFQE3_ERROR_OUT_OF_MEMORY;
        }
        else {
            ret = REFQE3_ERROR_CRYPTO;
        }
        goto ret_point;
    }
    memcpy(hash_drg_output, block, sizeof(sgx_cmac_128bit_tag_t));

    //Block 2 = AES-CMAC(Seal Key, QE ATT string with Counter = 0x02)
    content[0] = 0x02;
    if ((sgx_status = sgx_rijndael128_cmac_msg(reinterpret_cast<const sgx_cmac_128bit_key_t *>(*pkey_tmp),
        content,
        sizeof(content),
        &block)) != SGX_SUCCESS) {
        if (sgx_status == SGX_ERROR_OUT_OF_MEMORY) {
            ret = REFQE3_ERROR_OUT_OF_MEMORY;
        }
        else {
            ret = REFQE3_ERROR_CRYPTO;
        }
        goto ret_point;
    }
    memcpy(hash_drg_output + sizeof(sgx_cmac_128bit_tag_t), block, sizeof(sgx_cmac_128bit_tag_t));

    //Block 3 = AES-CMAC(Seal Key, QE ATT string with Counter = 0x03)
    content[0] = 0x03;
    if ((sgx_status = sgx_rijndael128_cmac_msg(reinterpret_cast<const sgx_cmac_128bit_key_t *>(*pkey_tmp),
        content,
        sizeof(content),
        &block)) != SGX_SUCCESS) {
        if (sgx_status == SGX_ERROR_OUT_OF_MEMORY) {
            ret = REFQE3_ERROR_OUT_OF_MEMORY;
        }
        else {
            ret = REFQE3_ERROR_CRYPTO;
        }
        goto ret_point;
    }
    //ECDSA Att Seed = most significant 320 bits of (Block 1 || Block 2 || Block 3).
    memcpy(hash_drg_output + 2 * sizeof(sgx_cmac_128bit_tag_t), block, HASH_DRBG_OUT_LEN - 2 * sizeof(sgx_cmac_128bit_tag_t));

    for (i = 0; i < HASH_DRBG_OUT_LEN / 2; i++) {//big endian to little endian
        hash_drg_output[i] ^= hash_drg_output[HASH_DRBG_OUT_LEN - 1 - i];
        hash_drg_output[HASH_DRBG_OUT_LEN - 1 - i] ^= hash_drg_output[i];
        hash_drg_output[i] ^= hash_drg_output[HASH_DRBG_OUT_LEN - 1 - i];
    }
    ref_static_assert(sizeof(g_sgx_nistp256_r_m1) == sizeof(sgx_ec256_private_t)); /*Unmatched size*/

        // Calculate pce private key
    if (sgx_calculate_ecdsa_priv_key((const unsigned char*)hash_drg_output,
        HASH_DRBG_OUT_LEN,
        (const unsigned char*)g_sgx_nistp256_r_m1,
        sizeof(g_sgx_nistp256_r_m1),
        (unsigned char*)p_att_priv_key,
        sizeof(sgx_ec256_private_t)) != SGX_SUCCESS) {
        ret = REFQE3_ERROR_CRYPTO;
        goto ret_point;
    }

    if (sgx_ecc256_calculate_pub_from_priv(p_att_priv_key, p_att_pub_key) != SGX_SUCCESS) {
        ret = REFQE3_ERROR_CRYPTO;
        goto ret_point;
    }

    //little endian to big endian
    SWAP_ENDIAN_32B(p_att_pub_key->gx);
    SWAP_ENDIAN_32B(p_att_pub_key->gy);

    ret = REFQE3_SUCCESS;

ret_point:
    //clear and free objects
    //
    (void)memset_s(pkey_tmp, sizeof(*pkey_tmp), 0, sizeof(*pkey_tmp));
    (void)memset_s(&hash_drg_output, sizeof(hash_drg_output), 0, sizeof(hash_drg_output));
    (void)memset_s(&block, sizeof(block), 0, sizeof(block));
    if (ret != REFQE3_SUCCESS) {
        (void)memset_s(p_att_priv_key, sizeof(sgx_ec256_private_t), 0, sizeof(sgx_ec256_private_t)); //clear private key in stack
        (void)memset_s(p_att_pub_key, sizeof(ref_ec256_public_t), 0, sizeof(ref_ec256_public_t)); //clear private key in stack
    }

    return ret;
}

/**
 * The QE_ID is a platform ID that is not associated with a particular SVN but is dependent on the Quoting Enclave's
 * (QE) MRSIGNER and its Seal Key.  The QE_ID is designed to be dependent on the seal key which is dependent on the
 * platform's OWNER_EPOCH value.  The OWNER_EPOCH value is set by the platform owner in the BIOS configuration.  If the
 * BIOS's non-volatile memory (FLASH) is wiped, then the QE_ID will change even if generated by the same QE.  This
 * prevents the QE_ID from being a true HW ID which cannot be modified by the platform owner.
 *
 * 1) QE_ID-Seed = EGETKEY(KEYNAME=SEAL_KEY,
 *                         KEY_POLICY=MRSIGNER,
 *                         KEY_ID = 0,
 *                         CPUSVN=0,
 *                         ISVSVN = 0)
 * 2) QE_ID = AES128-CMAC(QE_ID-Seed, 16 bytes below)
 *
 *    Byte Position  |  Value
 *        0          |  0x00
 *       1-9         |  "QE_ID_DER" (ascii encoded)
 *       10-13       |  0x00000000
 *       14-15       |  0x0080 (Big Endian)
 *
 * @param p_qe_id[Out] Pointer to the QE_ID.  Must not be NULL.
 *
 * @return REFQE3_SUCCESS Successfully created the QE_ID.
 * @return REFQE3_ERROR_INVALID_PARAMETER The QE_ID pointer is NULL.
 * @return REFQE3_ERROR_CRYPTO Error in the crypto library functions ues to generate the key.
 * @return REFQE3_ERROR_OUT_OF_MEMORY Heap memory was exhausted.
 *
 */
static qe3_error_t get_qe_id_internal(sgx_key_128bit_t *p_qe_id)
{
    sgx_status_t sgx_status = SGX_SUCCESS;
    qe3_error_t ret = REFQE3_SUCCESS;
    sgx_key_128bit_t key_tmp;
    sgx_key_request_t qe_id_key_req;

    // Defense-in-depth.  This is only called internally so should never be NULL
    if (NULL == p_qe_id) {
        return REFQE3_ERROR_INVALID_PARAMETER;
    }

    memset(&key_tmp, 0, sizeof(key_tmp));

    // Set up the key request structure for Seal Key with both CPUSVN and ISVSVN set to 0 and KeyID set to 0
    memset(&qe_id_key_req, 0, sizeof(sgx_key_request_t));
    qe_id_key_req.key_name = SGX_KEYSELECT_SEAL; // Seal key
    qe_id_key_req.key_policy = SGX_KEYPOLICY_MRSIGNER;
    qe_id_key_req.attribute_mask.xfrm = 0;
    qe_id_key_req.misc_mask = 0xFFFFFFFF;
    qe_id_key_req.attribute_mask.flags = ~SGX_FLAGS_MODE64BIT; //set all bits except the SGX_FLAGS_MODE64BIT
    sgx_status = sgx_get_key(&qe_id_key_req, &key_tmp);
    if (SGX_SUCCESS != sgx_status) {
        ret = REFQE3_ERROR_CRYPTO;
        goto ret_point;
    }

    uint8_t content[16];
    memset(&content, 0, sizeof(content));
    //1-10bytes: "QE_ID_DER"(ascii encoded)
    memcpy(content + 1, QE_ID_STRING, 9);
    //14-15bytes: 0x0080 (Big Endian)
    content[14] = 0x00;
    content[15] = 0x80;

    // Generate the mac as QE_ID
    ref_static_assert(sizeof(sgx_cmac_128bit_key_t) == sizeof(sgx_key_128bit_t));
    ref_static_assert(sizeof(sgx_cmac_128bit_tag_t) == sizeof(*p_qe_id));
    if ((sgx_status = sgx_rijndael128_cmac_msg(reinterpret_cast<const sgx_cmac_128bit_key_t *>(&key_tmp),
        content,
        sizeof(content),
        reinterpret_cast<sgx_cmac_128bit_tag_t *>(p_qe_id))) != SGX_SUCCESS) {
        if (sgx_status == SGX_ERROR_OUT_OF_MEMORY) {
            ret = REFQE3_ERROR_OUT_OF_MEMORY;
        }
        else {
            ret = REFQE3_ERROR_CRYPTO;
        }
    }
    else {
        ret = REFQE3_SUCCESS;
    }

ret_point:
    (void)memset_s(&key_tmp, sizeof(key_tmp), 0, sizeof(key_tmp)); //clear provisioning key in stack
    return ret;
}

/**
 * An internal function used to verify the ECDSA Blob.  It will verify the format of the blob and check the
 * authenticity using the seal key.  If the TCB of the platform has increased since the last time the blob was sealed,
 * it will be resealed to the new TCB and the p_is_resealed will be set to TRUE.  It will also optionally return the pub
 * key id and the encrypted data from the seal data if requested by the caller.
 *
 * @param p_blob    Pointer to the inputted ECDSA sealed blob to
 *                  be checked and unsealed.
 * @param blob_size Size in bytes of the ECDSA blob.
 * @param p_is_resealed
 *                  Pointer to return whether the blob was resealed to a new TCB.
 * @param p_plaintext_ecdsa_data
 *                  Pointer to the buffer that will contain the plaintext portion of the sealed blob.  This must not be
 *                  null.
 * @param p_pub_key_id
 *                  Optional pointer to the buffer to contain the ECDSA key ID stored in the blob.  If not NULL, it will
 *                  contain the ECDSA ID of the key in th blob upon returning.
 * @param p_report_body If non-NULL, it will contain the a non-targetted QE REPORT without any REPORT.ReportData.
 * @param pub_key_id_size
 *                  Size in bytes of the buffer pointed to by p_pub_key_id.  Ignored if p_pub_key_id is NULL but if not
 *                  NULL, it must be large enough to hold a SHA256 hash.
 * @param p_secret_ecdsa_data Optional pointer to a buffer that will contains the decrypted secret data
 *                                             in the sealed blob.
 *
 * @return REFQE3_SUCCESS ECDSA The blob verification passed and the p_secret_data will contain the decypted sealed
 *         secret data.
 * @return REFQE3_ECDSABLOB_ERROR There is a problem with the inputted blob format or unsealing failed.
 * @return REFQE3_ERROR_UNEXPECTED There is a problem retrieving the current platform TCB or resealing of the BLOB.
 * @return REFQE3_ERROR_OUT_OF_MEMORY Heap memory was exhausted.
 */
static qe3_error_t verify_blob_internal(uint8_t *p_blob,
    uint32_t blob_size,
    uint8_t *p_is_resealed,
    ref_plaintext_ecdsa_data_sdk_t *p_plaintext_ecdsa_data,
    sgx_report_body_t *p_report_body,
    uint8_t *p_pub_key_id,
    uint32_t pub_key_id_size,
    ref_ciphertext_ecdsa_data_sdk_t *p_secret_ecdsa_data)
{
    sgx_status_t sgx_status = SGX_SUCCESS;
    qe3_error_t ret = REFQE3_SUCCESS;
    uint8_t resealed = FALSE;
    //ref_ciphertext_ecdsa_data_sdk_t local_secret_ecdsa_data;
    //
    // securely align attestation key
    //
    sgx::custom_alignment_aligned<ref_ciphertext_ecdsa_data_sdk_t, 32, __builtin_offsetof(ref_ciphertext_ecdsa_data_sdk_t, ecdsa_private_key), sizeof(((ref_ciphertext_ecdsa_data_sdk_t*)0)->ecdsa_private_key)> osecret_ecdsa_data;
    ref_ciphertext_ecdsa_data_sdk_t* plocal_secret_ecdsa_data = &osecret_ecdsa_data.v;

    uint32_t plaintext_length;
    uint32_t decryptedtext_length = sizeof(*plocal_secret_ecdsa_data);
    sgx_sealed_data_t *p_ecdsa_blob = (sgx_sealed_data_t *)p_blob;
    uint8_t local_ecdsa_blob[SGX_QL_TRUSTED_ECDSA_BLOB_SIZE_SDK] = { 0 };
    sgx_report_t report;

    if ((NULL == p_plaintext_ecdsa_data) ||
        (NULL == p_is_resealed) ||
        (NULL == p_blob)) {
        return(REFQE3_ECDSABLOB_ERROR);
    }

    if (SGX_QL_TRUSTED_ECDSA_BLOB_SIZE_SDK != blob_size) {
        return(REFQE3_ECDSABLOB_ERROR);
    }
    if (NULL != p_pub_key_id) {
        if (pub_key_id_size < sizeof(sgx_sha256_hash_t)) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
    }
    if (sgx_get_encrypt_txt_len(p_ecdsa_blob) != sizeof(ref_ciphertext_ecdsa_data_sdk_t)) {
        return(REFQE3_ECDSABLOB_ERROR);
    }
    plaintext_length = sgx_get_add_mac_txt_len(p_ecdsa_blob);
    if (plaintext_length != sizeof(ref_plaintext_ecdsa_data_sdk_t)) {
        return(REFQE3_ECDSABLOB_ERROR);
    }

    memset(plocal_secret_ecdsa_data, 0, sizeof(*plocal_secret_ecdsa_data));
    memset(p_plaintext_ecdsa_data, 0, sizeof(*p_plaintext_ecdsa_data));

    sgx_status = sgx_unseal_data(p_ecdsa_blob,
        (uint8_t *)p_plaintext_ecdsa_data,
        &plaintext_length,
        (uint8_t *)plocal_secret_ecdsa_data,
        &decryptedtext_length);
    if (SGX_SUCCESS != sgx_status) {
        // The blob has been corrupted or the platform TCB has been downgraded.
        ret = REFQE3_ECDSABLOB_ERROR;
        goto ret_point;
    }

    if ((p_plaintext_ecdsa_data->seal_blob_type != SGX_QL_SEAL_ECDSA_KEY_BLOB)
        || (p_plaintext_ecdsa_data->ecdsa_key_version != SGX_QL_ECDSA_KEY_BLOB_VERSION_0)) {
        ret = REFQE3_ECDSABLOB_ERROR;
        goto ret_point;
    }

    if ((p_plaintext_ecdsa_data->ecdsa_key_version == SGX_QL_ECDSA_KEY_BLOB_VERSION_0 &&
        (decryptedtext_length != sizeof(ref_ciphertext_ecdsa_data_sdk_t) || plaintext_length != sizeof(ref_plaintext_ecdsa_data_sdk_t)))) {
        goto ret_point;
    }

    /* Create report to get current cpu_svn and isv_svn. */
    memset(&report, 0, sizeof(report));
    sgx_status = sgx_create_report(NULL, NULL, &report);
    if (SGX_SUCCESS != sgx_status) {
        if (SGX_ERROR_OUT_OF_MEMORY == sgx_status) {
            ret = REFQE3_ERROR_OUT_OF_MEMORY;
        }
        else {
            ret = REFQE3_ERROR_UNEXPECTED;
        }
        goto ret_point;
    }

    if (NULL != p_report_body) {
        memcpy(p_report_body, &report.body, sizeof(*p_report_body));
    }

    // Update the Key Blob using the SEAL Key for the current TCB if the TCB is
    // upgraded after the Key Blob is generated. Here memcmp cpu_svn might be
    // different even though they're actually the same, but for defense in depth we
    // will keep this comparison here. And we will also upgrade old format ECDSA
    // blob to new format here.
    if ((memcmp(&report.body.cpu_svn, &p_ecdsa_blob->key_request.cpu_svn, sizeof(report.body.cpu_svn))) ||
        (report.body.isv_svn != p_ecdsa_blob->key_request.isv_svn)) {
        sgx_status = sgx_seal_data(sizeof(*p_plaintext_ecdsa_data),
            (uint8_t*)p_plaintext_ecdsa_data,
            sizeof(*plocal_secret_ecdsa_data),
            (uint8_t*)plocal_secret_ecdsa_data,
            SGX_QL_TRUSTED_ECDSA_BLOB_SIZE_SDK,
            (sgx_sealed_data_t *)local_ecdsa_blob);
        if (SGX_SUCCESS != sgx_status) {
            if (SGX_ERROR_OUT_OF_MEMORY == sgx_status) {
                ret = REFQE3_ERROR_OUT_OF_MEMORY;
            }
            else {
                ret = REFQE3_ERROR_UNEXPECTED;
            }
            goto ret_point;
        }
        memcpy(p_ecdsa_blob, local_ecdsa_blob, blob_size);
        resealed = TRUE;
    }

    if (NULL != p_pub_key_id) {
        memcpy(p_pub_key_id, &p_plaintext_ecdsa_data->ecdsa_id, sizeof(p_plaintext_ecdsa_data->ecdsa_id));
    }
    if (NULL != p_secret_ecdsa_data) {
        memcpy(p_secret_ecdsa_data, plocal_secret_ecdsa_data, sizeof(*p_secret_ecdsa_data));
    }

ret_point:

    // Clear the output buffer to make sure nothing leaks.
    memset_s(plocal_secret_ecdsa_data, sizeof(*plocal_secret_ecdsa_data), 0, sizeof(*plocal_secret_ecdsa_data));
    if (REFQE3_SUCCESS == ret) {
        *p_is_resealed = resealed;
    }
    return ret;
}

/**
 * An external function exposed through the EDL to verify the ECDSA Blob.  It  will verify the format of the blob and
 * check the authenticity using the seal key.  If the TCB of the platform has increased since the last time the blob was
 * sealed, it will be resealed to the new TCB and the p_is_resealed will be set to TRUE.  It will also optionally return
 * the pub key id.
 *
 * @param p_blob [In, Out] Pointer to the ECDSA Blob. Must not be NULL and the full buffer must be inside the
 *                 enclave's memory space.  If the blob was resealed, upon return, the p_blob will point to the resealed
 *                 blob and the caller should save it.
 * @param blob_size [In] Size in bytes of the ECDSA blob buffer pointed to by p_blob.
 * @param p_is_resealed [In, Out] Pointer to flag that will be updated to true if the inputted blob is resealed.  This
 *                      will happen when the platform TCB has been increased since it  was last sealed.  Must not be
 *                      NULL.
 * @param p_report_body
 * @param pub_key_id_size [In] Size in bytes of the optional p_pub_key_id.  If p_pub_key_id is NULL, pub_key_id_size
 *                        must be zero.  The size must be large enough to contain a SHA256 hash.
 * @param p_pub_key_id [In, Out] Optional pointer to contain the ECDSA attestation public keys ID.  If not null, the
 *                     full buffer must inside the enclave's memory space.
 *
 * @return REFQE3_SUCCESS ECDSA The blob verification passed.
 * @return REFQE3_ERROR_OUT_OF_MEMORY Heap memory was exhausted.
 * @return REFQE3_ERROR_INVALID_PARAMETER One of the inputted parameters in invalid.
 * @return REFQE3_ECDSABLOB_ERROR There is a problem with the inputted blob format of unsealing.
 * @return REFQE3_ERROR_UNEXPECTED There is a problem retrieving the current platform TCB or resealing of the BLOB.
 * @return REFQE3_ECDSABLOB_ERROR  There is a problem with the inputted blob format of unsealing.
 */
uint32_t verify_blob(uint8_t *p_blob,
    uint32_t blob_size,
    uint8_t *p_is_resealed,
    sgx_report_body_t *p_report_body,
    uint32_t pub_key_id_size,
    uint8_t *p_pub_key_id)
{
    ref_plaintext_ecdsa_data_sdk_t plain_text;

    // Actually, some cases here will be checked with code generated by
    // edger8r. Here we just want to defend in depth.
    if (NULL == p_blob || NULL == p_is_resealed) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if (SGX_QL_TRUSTED_ECDSA_BLOB_SIZE_SDK != blob_size) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if (!sgx_is_within_enclave(p_blob, blob_size)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if ((pub_key_id_size != 0 && NULL == p_pub_key_id) ||
        (pub_key_id_size == 0 && NULL != p_pub_key_id)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if (NULL != p_pub_key_id) {
        if (pub_key_id_size < sizeof(sgx_sha256_hash_t)) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
        if (!sgx_is_within_enclave(p_pub_key_id, pub_key_id_size)) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
    }
    if (NULL != p_report_body) {
        if (!sgx_is_within_enclave(p_report_body, sizeof(*p_report_body))) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
    }

    return((uint32_t)random_stack_advance(verify_blob_internal,p_blob,
        blob_size,
        p_is_resealed,
        &plain_text,
        p_report_body,
        p_pub_key_id,
        pub_key_id_size,
        (ref_ciphertext_ecdsa_data_sdk_t*) NULL));
}

/**
 * External function exposed through the EDL used to return the QE3 report and the PPID encryption key required to get
 * the PCE identity information.  The PCE requires that the PPID be encrypted with a public key.  The reference supports
 * 1 type of certification data:
 *      1.  PPID_RSA3072_ENCRYPTED.
 * If PPID_CLEARTEXT was supported, the QE will generate an RSA3072 key pair and store both the public and private parts
 * in the enclave's global memory.  Reference Note: This requires that this function be called before the
 * store_cert_data() function in order to properly decrypt the PPID returned by the PCE and store it in the blob. This
 * function does not take the blob as input so the QE stores it in global memory.  If the QE is unloaded between this
 * function and the store_cert_data(), the ephemeral private key will be lost and the decryption of PPID in
 * store_cert_data() will fail. For a production version of the QE, the private key should be stored in the blob to keep
 * the QE stateless.  The platform software may consider storing the encryption public key if it intends to recertify
 * (request PCE to re-sign at a higher TCB) without calling the QE to re-generate an encryption key and a new QE REPORT.
 *
 * For PPID_RSA3072_ENCRYPTED, the QE will use the hardcoded public key owned by the quote verifier and store the PPID
 * encrypted by this RSA key in the ECDSA blob.
 *
 *
 * NOTE:  This is an optional function. If the other cert_key_types are used, the QE3 does not need to generate an
 * ephemeral PPID encryption key.
 *
 * @param p_pce_target_info
 *                 [In] Pointer to the target_info buffer of the PCE. It must not be NULL and the full target info
 *                 buffer must reside in the enclave's memory space.
 * @param p_qe3_report
 *                 [In, Out] Pointer to the QE report buffer targeting the PCE. It must not be NULL and full report
 *                 buffer must reside in the enclave's memory space.
 * @param crypto_suite
 *                 [In] Indicates the crypto algorithm to use to encrypt the PPID. Currently, only RSA3072 keys are
 *                 supported.  This is the type of key this function will generate.
 * @param cert_key_type
 *                 [In] Indicates whether to use the hard-coded public key or generate a new one.  This option allows
 *                  the reference to demonstrate creating an encryption key on-demand or to use the hard-coded value.
 *                  Using the hard-coded value typically means the PPID is to remain private on the platform. Must be
 *                  PPID_RSA3072_ENCRYPTED.
 *
 * @param key_size [In] The size in bytes of the supplied p_public_key buffer.  Currently, it must be equal to the size
 *                 of an RSA3072 public key. 4 byte 'e' and 256 byte 'n'.
 * @param p_public_key
 *                 [In, Out] Pointer to the buffer that will contain the public key used to encrypt the PPID. It must
 *                 not be NULL and the buffer must reside within the enclave's memory space.
 *
 * @return REFQE3_SUCCESS Function successfully generated or retrieved the encryption key and generated a REPORT
 *         targeting the PCE.
 * @return REFQE3_ERROR_INVALID_PARAMETER Invalid parameter.
 * @return REFQE3_ERROR_CRYPTO Error generated the encryption key.
 * @return REFQE3_ERROR_UNEXPECTED An internal error occurred.
 */
uint32_t get_pce_encrypt_key(const sgx_target_info_t* p_pce_target_info,
    sgx_report_t* p_qe3_report,
    uint8_t crypto_suite,
    uint16_t cert_key_type,
    uint32_t key_size,
    uint8_t* p_public_key)
{
    qe3_error_t ret = REFQE3_SUCCESS;
    sgx_status_t sgx_status = SGX_SUCCESS;
    sgx_report_data_t report_data = { 0 };
    sgx_sha_state_handle_t sha_handle = NULL;
    pce_rsaoaep_3072_encrypt_pub_key_t* p_rsa_pub_key;

    if (p_pce_target_info == NULL || !sgx_is_within_enclave(p_pce_target_info, sizeof(*p_pce_target_info))) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if (p_public_key == NULL || !sgx_is_within_enclave(p_public_key, key_size)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if (p_qe3_report == NULL || !sgx_is_within_enclave(p_qe3_report, sizeof(*p_qe3_report))) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if (crypto_suite != PCE_ALG_RSA_OAEP_3072) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if (key_size != sizeof(*p_rsa_pub_key)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
    // Only PPID_RSA3072_ENCRYPTED is supported when using production mode PCE.
    if (PPID_RSA3072_ENCRYPTED != cert_key_type) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    // During development of QE3, the PCE used may be both launched in debug mode and not have the
    // provisioning bit set.  These checks should be put back in when the QE3 is production worthy and on the white
    // list.
#ifndef ALLOW_DEBUG_PCE
    if ((p_pce_target_info->attributes.flags & SGX_FLAGS_PROVISION_KEY) != SGX_FLAGS_PROVISION_KEY ||
        (p_pce_target_info->attributes.flags & SGX_FLAGS_DEBUG) != 0)
    {
        //PCE must have access to provisioning key
        //Can't be debug PCE
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
#endif

    // PPID_CLEARTEXT not supported.  Input check will not allow it to get here.
    if (PPID_CLEARTEXT == cert_key_type) {
#ifdef ALLOW_CLEARTEXT_PPID
        g_rsa_key.e[0] = 0x10001;
        p_rsa_pub_key = (pce_rsaoaep_3072_encrypt_pub_key_t*)p_public_key;
        //todo: Currenlty, the private key is stored temporarily in enclave global memory long enough
        // to last between get_pce_encrypt_key() and store_cert_data().  These calls surround the call to the PCE
        // get_pce_info() API.  There is a risk that if the enclave is unloaded directly or indirectly (by power state
        // change) the private key will be lost.  There should be more documentation about this situation w/r/t
        // detection and recovery.  Or, if that is not sufficient, then provide a way to store the key in the ECDSA
        // blob.  Since PPID_CLEARTEXT cert_key_type is not supported at this time, we can push the solution for later.
        sgx_status_t ret_code = sgx_create_rsa_key_pair(REF_RSA_OAEP_3072_MOD_SIZE,
            REF_RSA_OAEP_3072_EXP_SIZE,
            (unsigned char*)g_rsa_key.n,
            (unsigned char*)g_rsa_key.d,
            (unsigned char*)g_rsa_key.e,
            (unsigned char*)g_rsa_key.p,
            (unsigned char*)g_rsa_key.q,
            (unsigned char*)g_rsa_key.dmp1,
            (unsigned char*)g_rsa_key.dmq1,
            (unsigned char*)g_rsa_key.iqmp);
        if (ret_code != SGX_SUCCESS) {
            ret = REFQE3_ERROR_CRYPTO;
            goto ret_point;
        }

        // PCE wants the key in big endian
        size_t i;
        uint8_t* p_temp;
        p_temp = (uint8_t*)g_rsa_key.e;
        for (i = 0; i < REF_RSA_OAEP_3072_EXP_SIZE; i++) {
            p_rsa_pub_key->e[i] = *(p_temp + REF_RSA_OAEP_3072_EXP_SIZE - 1 - i); //create big endian e
        }
        p_temp = (uint8_t*)g_rsa_key.n;
        for (i = 0; i < REF_RSA_OAEP_3072_MOD_SIZE; i++) {
            p_rsa_pub_key->n[i] = *(p_temp + REF_RSA_OAEP_3072_MOD_SIZE - 1 - i); //create big endian n
        }
#else
        //Shouldn't get here based on cert_key_type input parameter check
        ret = REFQE3_ERROR_UNEXPECTED;
        goto ret_point;
#endif
    }
    else if (PPID_RSA3072_ENCRYPTED == cert_key_type) {
        p_rsa_pub_key = (pce_rsaoaep_3072_encrypt_pub_key_t*)p_public_key;
        memcpy(p_rsa_pub_key->e, g_ref_pubkey_e_be, sizeof(p_rsa_pub_key->e));
        memcpy(p_rsa_pub_key->n, g_ref_pubkey_n_be, sizeof(p_rsa_pub_key->n));
    }
    else {
        ret = REFQE3_ERROR_INVALID_PARAMETER;
        goto ret_point;
    }

    // report_data = SHA256(crypto_suite||rsa_pub_key)||0-padding
    do {
        sgx_status = sgx_sha256_init(&sha_handle);
        if (SGX_SUCCESS != sgx_status)
            break;

        sgx_status = sgx_sha256_update(&crypto_suite,
            sizeof(uint8_t),
            sha_handle);
        if (SGX_SUCCESS != sgx_status)
            break;
        //(MOD followed by e)
        sgx_status = sgx_sha256_update(p_rsa_pub_key->n,
            sizeof(p_rsa_pub_key->n),
            sha_handle);
        if (SGX_SUCCESS != sgx_status)
            break;
        sgx_status = sgx_sha256_update(p_rsa_pub_key->e,
            sizeof(p_rsa_pub_key->e),
            sha_handle);
        if (SGX_SUCCESS != sgx_status)
            break;
        sgx_status = sgx_sha256_get_hash(sha_handle,
            reinterpret_cast<sgx_sha256_hash_t *>(&report_data));
    } while (0);
    if (SGX_SUCCESS != sgx_status) {
        ret = REFQE3_ERROR_UNEXPECTED;
        goto ret_point;
    }

    sgx_status = sgx_create_report(p_pce_target_info, &report_data, p_qe3_report);
    if (SGX_SUCCESS != sgx_status) {
        if (SGX_ERROR_OUT_OF_MEMORY == sgx_status) {
            ret = REFQE3_ERROR_OUT_OF_MEMORY;
        }
        else {
            ret = REFQE3_ERROR_UNEXPECTED;
        }
        goto ret_point;
    }

ret_point:
    // Clear critical output data on error
    if (REFQE3_SUCCESS != ret) {
        memset_s(p_qe3_report, sizeof(*p_qe3_report), 0, sizeof(*p_qe3_report));
#ifdef ALLOW_CLEARTEXT_PPID
        memset_s(&g_rsa_key, sizeof(g_rsa_key), 0, sizeof(g_rsa_key));
#endif
    }
    if (sha_handle != NULL) {
        sgx_sha256_close(sha_handle);
    }

    return(ret);
}

/**
 * External function exposed through the EDL to generate the ECDSA attestation key.  The generated key will be stored in
 * the ECDSA blob passed in to the function.  The public part of the key is MAC'd and the private key is encrypted and
 * MAC'd with the QE sealing key.  First the attestation key is generated and its
 * SHA256(public_key||authendication_data) is added to the returned QE REPORT.report_data. The caller can then send the
 * report to the PCE to be certified.  Once certified, the certification data is given back to this enclave
 * (store_cert_data()) to be sealed along with the attestation key in this fucntion.  This function must be called
 * before the store_cert_data() function unless the store_cert_data() function is only called to re-certify an existing
 * key already in an existing ECDSA blob. If the store_cert_data() function is called without first generating the key,
 * the ECDSA blob verification will fail or the quote signatures will not match PCE certified attestation keys.
 *
 * Any data in the p_blob will be overwritten when this fuction is called.
 *
 * @param p_blob    [In, Out] Pointer to the ECDSA Blob. Must not be NULL and the full buffer must be inside the
 *                  enclave's memory space.  If the blob was resealed, upon return, the p_blob will point to the
 *                  resealed blob and the caller should save it.[in, out] Pointer to the ECDSA Blob.
 * @param blob_size [In] Size in bytes of the ECDSA blob buffer pointed to by p_blob.
 * @param p_pce_target_info
 *                  [In] Pointer to the target_info of the PCE. It must not be NULL and must reside in the enclave's memory
 *                  space.
 * @param p_qe3_report
 *                  [In, Out] Pointer to the outputted QE report targeting the PCE. The Report's ReportData will contain
 *                  the SHA256 hash of the ECDSA public key.  The hash is generated using the big endian formatted ECDSA
 *                  public key. SHA256(X||Y) where both X and Y are in Big Endian format.
 * @param p_authentication_data
 *                   [In] Optional pointer to the extra authentication data provided by the caller. This data will be
 *                   included in the quote and signed by the ECDSA attestation.  If provided the
 *                   authentication_data_size must not be zero and the full buffer must reside inside the enclave's
 *                   memory space.
 * @param authentication_data_size
 *                   [In]  The size in bytes of the data pointed to by p_authentication_data. If this value must be zero
 *                   if the p_authentication_data pointer is NULL and it must be non-zero of it is not NULL.
 *
 * @return REFQE3_SUCCESS The attestation keys was successfully generated.  The blob will have the ECDSA key and be
 *                        sealed.  The QE Report targeting the PCE is returned with the ReportData containing the hash
 *                        of the ECDSA attestation** public key and authentication data.
 * @return REFQE3_ERROR_INVALID_PARAMETER
 * @return REFQE3_ERROR_OUT_OF_MEMORY
 * @return REFQE3_ERROR_ATT_KEY_GEN
 * @return REFQE3_ERROR_UNEXPECTED
 */
uint32_t gen_att_key(uint8_t *p_blob,
    uint32_t blob_size,
    const sgx_target_info_t *p_pce_target_info,
    sgx_report_t *p_qe3_report,
    uint8_t *p_authentication_data,
    uint32_t authentication_data_size)
{
    qe3_error_t ret = REFQE3_SUCCESS;
    sgx_status_t sgx_status = SGX_SUCCESS;
#ifdef GENERATE_RANDOM_ATTESTATION_KEY
    sgx_ecc_state_handle_t ecc_handle = NULL;
#endif
    sgx_sha_state_handle_t sha_handle = NULL;
    sgx_report_data_t report_data = { 0 };
    ref_plaintext_ecdsa_data_sdk_t plaintext_data;
    //
    // provide extra protection for attestation key by
    // randomizing its address and securely aligning it
    //
    using cciphertext_data = randomly_placed_object<
        sgx::custom_alignment_aligned<
        ref_ciphertext_ecdsa_data_sdk_t,
        alignof(ref_ciphertext_ecdsa_data_sdk_t),
        __builtin_offsetof(ref_ciphertext_ecdsa_data_sdk_t, ecdsa_private_key),
        sizeof(((ref_ciphertext_ecdsa_data_sdk_t*)0)->ecdsa_private_key)>>;
    //
    // instance of randomly_placed_object
    //
    cciphertext_data ociphertext_data_buf;
    //
    // pointer to instance of custom_alignment_aligned
    //
    auto* ociphertext_data = ociphertext_data_buf.instantiate_object();

    ref_ciphertext_ecdsa_data_sdk_t* pciphertext_data = &ociphertext_data->v;

    sgx_key_id_t req_key_id = { 0 };

    if ((NULL == p_blob) ||
        (NULL == p_pce_target_info) ||
        (NULL == p_qe3_report))
        return(REFQE3_ERROR_INVALID_PARAMETER);

    if (SGX_QL_TRUSTED_ECDSA_BLOB_SIZE_SDK != blob_size) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    // Check whether p_blob is copied into EPC. If we want to reduce the
    // memory usage, maybe we can leave the p_blob outside EPC.
    if (!sgx_is_within_enclave(p_blob, blob_size)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if (!sgx_is_within_enclave(p_pce_target_info, sizeof(*p_pce_target_info))) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if (!sgx_is_within_enclave(p_qe3_report, sizeof(*p_qe3_report))) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    // During development of QE3, the PCE used may be both launched in debug mode and not have the
    // provisioning bit set.  These checks should be put back in when the QE3 is production worthy and on
    // the white list.
#ifndef ALLOW_DEBUG_PCE
    if ((p_pce_target_info->attributes.flags & SGX_FLAGS_PROVISION_KEY) !=
        SGX_FLAGS_PROVISION_KEY || (p_pce_target_info->attributes.flags & SGX_FLAGS_DEBUG) != 0)
    {
        //PCE must have access to provisioning key
        //Can't be debug PCE
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
#endif

    // ECDSA quotes supports 'authentication_data' that will be signed by the PCE's PCK along with the ECDSA Attestation key
    if ((0 != authentication_data_size) && (NULL == p_authentication_data)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
    if ((0 == authentication_data_size) && (NULL != p_authentication_data)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
    if (p_authentication_data) {
        if (!sgx_is_within_enclave(p_authentication_data, authentication_data_size)) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
        ///todo:  The intention is to allow this data to be truly variable.  This check forces it to be a fixed size.
        //Make ake the necessary changes to fully support a variable size in the future.
        if (REF_ECDSDA_AUTHENTICATION_DATA_SIZE != authentication_data_size) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
    }

    memset(&plaintext_data, 0, sizeof(plaintext_data));

    if (UINT16_MAX < authentication_data_size) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
    plaintext_data.authentication_data_size = (uint16_t)authentication_data_size;
    if (p_authentication_data) {
        sgx_lfence();
        memcpy(plaintext_data.authentication_data, p_authentication_data, sizeof(plaintext_data.authentication_data));
    }

#ifdef GENERATE_RANDOM_ATTESTATION_KEY
    sgx_status = sgx_ecc256_open_context(&ecc_handle);
    if (SGX_ERROR_OUT_OF_MEMORY == sgx_status)
        return(REFQE3_ERROR_OUT_OF_MEMORY);
    if (SGX_SUCCESS != sgx_status)
        return(REFQE3_ERROR_ATT_KEY_GEN);

    sgx_ecc256_create_key_pair(&pciphertext_data->ecdsa_private_key,
        (sgx_ec256_public_t*)&plaintext_data.ecdsa_att_public_key,
        ecc_handle);
    if (SGX_SUCCESS != sgx_status) {
        ret = REFQE3_ERROR_ATT_KEY_GEN;
        goto ret_point;
    }
    // Private and Public key are return in little endian format.
    // Store public key and generate the hash using big endian format.
    // Store the Private key is little endian as the signing algorithm is uses little endian format.
    {
        size_t i;
        uint8_t swap;
        for (i = 0; i < 32 / 2; i++) {
            swap = plaintext_data.ecdsa_att_public_key.gx[i];
            plaintext_data.ecdsa_att_public_key.gx[i] = plaintext_data.ecdsa_att_public_key.gx[32 - 1 - i];
            plaintext_data.ecdsa_att_public_key.gx[32 - 1 - i] = swap;
        }
        for (i = 0; i < 32 / 2; i++) {
            swap = plaintext_data.ecdsa_att_public_key.gy[i];
            plaintext_data.ecdsa_att_public_key.gy[i] = plaintext_data.ecdsa_att_public_key.gy[32 - 1 - i];
            plaintext_data.ecdsa_att_public_key.gy[32 - 1 - i] = swap;
        }
    }
#else
    ret = random_stack_advance(get_att_key_based_from_seal_key,&pciphertext_data->ecdsa_private_key,
        &plaintext_data.ecdsa_att_public_key,
        &req_key_id);
    if (REFQE3_SUCCESS != ret) {
        if (REFQE3_ERROR_CRYPTO == ret) {
            ret = REFQE3_ERROR_ATT_KEY_GEN;
        }
        goto ret_point;
    }
    // Public key is returned in big endian format. Store public key and generate the hash using big
    // endian format.
    // The Private key is returned in little endian format. Store the private key is little endian
    // as the signing algorithm is uses little endian format.
#endif

    do {
        sgx_status = sgx_sha256_init(&sha_handle);
        if (SGX_SUCCESS != sgx_status) {
            break;
        }

        sgx_status = sgx_sha256_update((uint8_t*)&plaintext_data.ecdsa_att_public_key,
            sizeof(plaintext_data.ecdsa_att_public_key),
            sha_handle);
        if (SGX_SUCCESS != sgx_status) {
            break;
        }

        sgx_status = sgx_sha256_update((uint8_t*)plaintext_data.authentication_data,
            sizeof(plaintext_data.authentication_data),
            sha_handle);
        if (SGX_SUCCESS != sgx_status) {
            break;
        }

        sgx_status = sgx_sha256_get_hash(sha_handle,
            reinterpret_cast<sgx_sha256_hash_t *>(&plaintext_data.ecdsa_id));
        if (SGX_SUCCESS != sgx_status) {
            break;
        }

    } while (0);
    if (SGX_SUCCESS != sgx_status) {
        ret = REFQE3_ERROR_UNEXPECTED;
        goto ret_point;
    }
    ref_static_assert(sizeof(plaintext_data.ecdsa_id) <= sizeof(report_data));
    memcpy(&report_data, &plaintext_data.ecdsa_id, sizeof(plaintext_data.ecdsa_id));

    sgx_status = sgx_create_report(p_pce_target_info, &report_data, p_qe3_report);
    if (SGX_SUCCESS != sgx_status) {
        if (SGX_ERROR_OUT_OF_MEMORY == sgx_status) {
            ret = REFQE3_ERROR_OUT_OF_MEMORY;
        }
        else {
            ret = REFQE3_ERROR_UNEXPECTED;
        }
        goto ret_point;
    }

    // Add QE_ID to the ECDSA Blob
    ret = get_qe_id_internal(&plaintext_data.qe3_id);
    if (REFQE3_SUCCESS != ret) {
        goto ret_point;
    }

    plaintext_data.seal_blob_type = SGX_QL_SEAL_ECDSA_KEY_BLOB;
    plaintext_data.ecdsa_key_version = SGX_QL_ECDSA_KEY_BLOB_VERSION_0;
    // Call sgx_seal_data to generate the ECDSA Blob with the updated information
    sgx_status = sgx_seal_data(sizeof(plaintext_data),
        reinterpret_cast<uint8_t*>(&plaintext_data),  //plaintext as AAD
        sizeof(*pciphertext_data),
        reinterpret_cast<uint8_t*>(pciphertext_data), //ciphertext data to SEAL
        blob_size,
        (sgx_sealed_data_t*)p_blob);
    if (SGX_SUCCESS != sgx_status) {
        ret = (qe3_error_t)sgx_status;
        goto ret_point;
    }

ret_point:
    // Clear output data on error
    if (REFQE3_SUCCESS != ret) {
        (void)memset_s(p_qe3_report, sizeof(*p_qe3_report), 0, sizeof(*p_qe3_report));
    }

#ifdef GENERATE_RANDOM_ATTESTATION_KEY
    if (NULL != ecc_handle) {
        sgx_status = sgx_ecc256_close_context(ecc_handle);
    }
#endif
    if (NULL != sha_handle) {
        sgx_sha256_close(sha_handle);
    }

    // Clear out any sensitive data from the stack before returning.
    memset_s(pciphertext_data, sizeof(*pciphertext_data), 0, sizeof(*pciphertext_data));
    return(ret);
}

/**
 * External function exposed through the EDL used to store the ECDSA blob with all of the certification data from the
 * PCE along with the ECDSA attestation key.  It must be called after retrieving the PCE identity information, generating
 * the ECDSA attestation key pair, and getting the PCE to certify the ECDSA attestation key. Once the ECDSA blob has been
 * stored with all of the requisite information, quote generation can take place.
 *
 * @param p_plaintext_data
 *                  [In] Pointer to the plaintext data to store in the blob. Most of the plaintext values are populated
 *                  by the calling code. It must not be NULL and the full plaintext buffer must reside in the enclave's
 *                  memory space.
 * @param cert_key_type
 *                  [In] Indicates the type of certification data used in the quote.  Only PPID_RSA3072 is supported. If
 *                  PPID_CLEARTEXT was supported, the PPID will be decrypted by the RSA private before sealing
 *                  (encrypted) it in the ECDSA blob  with the seal key.  Otherwise, the encrypted PPID is left
 *                  encrypted by the RSA key and the sealed (encrypted) by the seal key.
 * @param p_encrypted_ppid
 *                  [In] Pointer to the encrypted PPID as generated by the PCE.
 * @param encrypted_ppid_size
 *                  [In] Size of the enrypted PPID.
 * @param p_blob    [In, Out]  Pointer the buffer that will contain the sealed ECDSA blob. For recertification, it will
 *                  contain a valid ECDSA blob that will be unsealed to extract the secret data and then be resealed
 *                  with the new certification data. Otherwise, the contents of the p_blob buffer will be overwritten
 *                  with the new certification data and new secret data. It must not be NULL and the buffer must reside
 *                  in the enclave's memory space.
 * @param blob_size [In] Size in bytes of the p_blob buffer.  Must be equal to SGX_QL_TRUSTED_ECDSA_BLOB_SIZE_SDK.
 *
 * @return REFQE3_SUCCESS
 * @return REFQE3_ERROR_INVALID_PARAMETER
 * @return REFQE3_ERROR_UNEXPECTED
 * @return REFQE3_ERROR_OUT_OF_MEMORY
 * @return REFQE3_ERROR_CRYPTO
 * @return REFQE3_ECDSABLOB_ERROR
 */
uint32_t store_cert_data(ref_plaintext_ecdsa_data_sdk_t *p_plaintext_data,
    sgx_ql_cert_key_type_t cert_key_type,
    uint8_t *p_encrypted_ppid,
    uint32_t encrypted_ppid_size,
    uint8_t *p_blob,
    uint32_t blob_size)
{
    qe3_error_t ret = REFQE3_SUCCESS;
    sgx_status_t sgx_status = SGX_SUCCESS;
    //
    // provide extra protection for attestation key by
    // randomizing its address and securely aligning it
    //
    using cciphertext_data = randomly_placed_object<
        sgx::custom_alignment_aligned<
        ref_ciphertext_ecdsa_data_sdk_t,
        alignof(ref_ciphertext_ecdsa_data_sdk_t),
        __builtin_offsetof(ref_ciphertext_ecdsa_data_sdk_t, ecdsa_private_key),
        sizeof(((ref_ciphertext_ecdsa_data_sdk_t*)0)->ecdsa_private_key)>>;
    //
    // instance of randomly_placed_object
    //
    cciphertext_data ociphertext_data_buf;
    //
    // pointer to instance of custom_alignment_aligned
    //
    auto* ociphertext_data = ociphertext_data_buf.instantiate_object();
    ref_ciphertext_ecdsa_data_sdk_t* pciphertext_data = &ociphertext_data->v;

#ifdef ALLOW_CLEARTEXT_PPID
    void *rsa_key = NULL;
    unsigned char* dec_dat = NULL;
    size_t ppid_size = 0;
#endif
    ref_plaintext_ecdsa_data_sdk_t local_plaintext_data;
    uint8_t is_resealed;

    if ((NULL == p_blob) ||
        (NULL == p_plaintext_data)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
    if (blob_size != SGX_QL_TRUSTED_ECDSA_BLOB_SIZE_SDK) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
    /* Check whether p_blob is copied into EPC. If we want to reduce the
       memory usage, maybe we can leave the p_blob outside EPC. */
    if (!sgx_is_within_enclave(p_blob, blob_size)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if (NULL != p_encrypted_ppid) {
        if ((!sgx_is_within_enclave(p_encrypted_ppid, REF_RSA_OAEP_3072_MOD_SIZE)) ||
            (REF_RSA_OAEP_3072_MOD_SIZE != encrypted_ppid_size)) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
    }

    if (!sgx_is_within_enclave(p_plaintext_data, sizeof(*p_plaintext_data))) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    // Only 3072 encrypted PPID is supported post alpha.
    if (PPID_RSA3072_ENCRYPTED != cert_key_type) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    // Verify EPID p_blob
    ret = random_stack_advance(verify_blob_internal,p_blob,
        blob_size,
        &is_resealed,
        &local_plaintext_data,
        (sgx_report_body_t*) NULL,
        (uint8_t*) NULL,
        0,
        pciphertext_data);
    if (REFQE3_SUCCESS != ret) {
        goto ret_point;
    }

    // Compare the ECDSA_ID passed in matches the value in the existing ECDSA Blob.  This should catch keys that haven't been generated before storing
    if (0 != memcmp(&local_plaintext_data.ecdsa_id, &p_plaintext_data->qe3_report.body.report_data, sizeof(local_plaintext_data.ecdsa_id))) { //ECDSA_ID is the first 32bytes or REPORT.ReportData
        ret = REFQE3_ERROR_INVALID_PARAMETER;
        goto ret_point;
    }

    /* Create report to get current cpu_svn and isv_svn. */
    sgx_report_t report;
    memset(&report, 0, sizeof(report));
    sgx_status = sgx_create_report(NULL, NULL, &report);
    if (SGX_SUCCESS != sgx_status) {
        if (SGX_ERROR_OUT_OF_MEMORY == sgx_status) {
            ret = REFQE3_ERROR_OUT_OF_MEMORY;
        }
        else {
            ret = REFQE3_ERROR_UNEXPECTED;
        }
        goto ret_point;
    }

    /* Store the current QE PSVN with the blob to indicate what the TCB was when sealed. */
    memcpy(&local_plaintext_data.seal_cpu_svn, &report.body.cpu_svn, sizeof(local_plaintext_data.seal_cpu_svn));
    local_plaintext_data.seal_qe3_isv_svn = report.body.isv_svn;

    // For recertification, the PPID does not change.  No need to process the PPID information again since it is
    // unchanged from the previous certification.
    // PPID_CLEARTEXT is not supported.  Parameter check above will not allow this cert_key_type.
    if (NULL != p_encrypted_ppid) {
        if (PPID_CLEARTEXT == cert_key_type) {
#ifdef ALLOW_CLEARTEXT_PPID
            // Decrypt the PPID with the RSA private key generated with the new key and store it in the blob
            // Create a private key context
            /// todo: add a check to see if the private key was lost due to enlave unload or power loss.
            if (SGX_SUCCESS != sgx_create_rsa_priv2_key(REF_RSA_OAEP_3072_MOD_SIZE,
                REF_E_SIZE_IN_BYTES,
                (const unsigned char*)g_rsa_key.e,
                (const unsigned char*)g_rsa_key.p,
                (const unsigned char*)g_rsa_key.q,
                (const unsigned char*)g_rsa_key.dmp1,
                (const unsigned char*)g_rsa_key.dmq1,
                (const unsigned char*)g_rsa_key.iqmp,
                &rsa_key)) {
                ret = REFQE3_ERROR_CRYPTO;
                goto ret_point;
            }
            if (SGX_SUCCESS != sgx_rsa_priv_decrypt_sha256(rsa_key,
                NULL,
                (&ppid_size),
                p_encrypted_ppid,
                REF_RSA_OAEP_3072_MOD_SIZE)) {
                ret = REFQE3_ERROR_CRYPTO;
                goto ret_point;
            }
            //if (sizeof(ciphertext_data.ppid) < ppid_size) {
            //    ret = REFQE3_ERROR_CRYPTO;
            //    goto ret_point;
            //}
            if (!(dec_dat = (unsigned char*)malloc(ppid_size))) {
                ret = REFQE3_ERROR_CRYPTO;
                goto ret_point;
            }
            if (SGX_SUCCESS != sgx_rsa_priv_decrypt_sha256(rsa_key,
                dec_dat,
                (&ppid_size),
                p_encrypted_ppid,
                REF_RSA_OAEP_3072_MOD_SIZE)) {
                ret = REFQE3_ERROR_CRYPTO;
                goto ret_point;
            }
            // Copy in the decrypted PPID
            pciphertext_data->is_clear_ppid = 1;   // Specifies that ciphertext part of the blob contains the cleartext PPID instead of the encrypted PPID
            memcpy(pciphertext_data->ppid, dec_dat, sizeof(pciphertext_data->ppid));
#else
            //Shouldn't get here based on cert_key_type input parameter check
            ret = REFQE3_ERROR_UNEXPECTED;
            goto ret_point;
#endif
        }
        else {
            pciphertext_data->is_clear_ppid = 0;   // Specifies that ciphertext part of the blob contains the ciphertext PPID instead of the cleartext PPID
            pciphertext_data->encrypted_ppid_data.crypto_suite = PCE_ALG_RSA_OAEP_3072;
            pciphertext_data->encrypted_ppid_data.encrypted_ppid_buf_size = encrypted_ppid_size;
            memcpy(pciphertext_data->encrypted_ppid_data.encrypted_ppid, p_encrypted_ppid, encrypted_ppid_size); //encrypted_ppid_size checked above.
        }
    }

    local_plaintext_data.cert_qe3_isv_svn = report.body.isv_svn;

    // Copy in the PCE identity used to certify the ECDSA Attestation key
    memcpy(&local_plaintext_data.cert_cpu_svn, &p_plaintext_data->cert_cpu_svn, sizeof(local_plaintext_data.cert_cpu_svn));
    local_plaintext_data.cert_pce_info.pce_isv_svn = p_plaintext_data->cert_pce_info.pce_isv_svn;
    local_plaintext_data.cert_pce_info.pce_id = p_plaintext_data->cert_pce_info.pce_id;

    // Re-copy in the old certification data
    local_plaintext_data.signature_scheme = p_plaintext_data->signature_scheme;
    memcpy(&local_plaintext_data.qe3_report, &p_plaintext_data->qe3_report, sizeof(local_plaintext_data.qe3_report));
    memcpy(&local_plaintext_data.qe3_report_cert_key_sig, &p_plaintext_data->qe3_report_cert_key_sig, sizeof(local_plaintext_data.qe3_report_cert_key_sig));
    local_plaintext_data.certification_key_type = p_plaintext_data->certification_key_type;
    memcpy_s(&local_plaintext_data.pce_target_info, sizeof(local_plaintext_data.pce_target_info), &p_plaintext_data->pce_target_info, sizeof(p_plaintext_data->pce_target_info));
    memcpy_s(&local_plaintext_data.raw_cpu_svn, sizeof(local_plaintext_data.raw_cpu_svn), &p_plaintext_data->raw_cpu_svn, sizeof(p_plaintext_data->raw_cpu_svn));
    local_plaintext_data.raw_pce_info.pce_isv_svn = p_plaintext_data->raw_pce_info.pce_isv_svn;
    local_plaintext_data.raw_pce_info.pce_id = p_plaintext_data->raw_pce_info.pce_id;

    // Call sgx_seal_data to generate the ECDSA Blob with the updated information
    sgx_status = sgx_seal_data(sizeof(local_plaintext_data),
        reinterpret_cast<uint8_t*>(&local_plaintext_data),  //plaintext as AAD
        sizeof(*pciphertext_data),
        reinterpret_cast<uint8_t*>(pciphertext_data), //ciphertext data to SEAL
        blob_size,
        (sgx_sealed_data_t*)p_blob);
    if (SGX_SUCCESS != sgx_status) {
        ret = (qe3_error_t)sgx_status;
        goto ret_point;
    }

ret_point:
    memset_s(pciphertext_data, sizeof(*pciphertext_data), 0, sizeof(*pciphertext_data));
#ifdef ALLOW_CLEARTEXT_PPID
    if (NULL != rsa_key) {
        sgx_free_rsa_key(rsa_key, SGX_RSA_PRIVATE_KEY, REF_RSA_OAEP_3072_MOD_SIZE, 0);
    }
    if (NULL != dec_dat) {
        memset_s(dec_dat, ppid_size, 0, ppid_size);
        free(dec_dat);
    }
#endif
    return(ret);
}

/**
 * External function exposed through the EDL used to generate a quote.  It will take the REPORT (targeting this enclave)
 * of the application enclave requesting a quote.  It also takes the ECDSA blob that contains the private attestation
 * key and the PCE certification data.  The other inputs are the data required to fill out the quote structure. The
 * application enclave's report will verified and signed with the ECDSA attestation private key and will return the
 * resulting quote in the provided buffer. The caller can also optionally request that this enclave generate a report
 * targeting the requesting enclave along with a nonce.  If requested, the resulting QE report will be returned with
 * the QE_Report.ReportData equal to the nonce. This allows the requesting enclave to verify the Quote was generated by
 * a valid QE.
 *
 * @param p_blob     [In, Out] Pointer to the ECDSA Blob. Must not be NULL and the full buffer must be inside the
 *                   enclave's memory space.  If the blob was resealed, upon return, the p_blob will point to the
 *                   resealed blob and the caller should save it.[in, out] Pointer to the ECDSA Blob.
 * @param blob_size  [In] Size in bytes of the ECDSA blob buffer pointed to by p_blob.
 * @param p_enclave_report
 *                   [In] The application enclave's report.  It must be generated targeting this enclave.
 * @param p_nonce    [In] Optional pointer to a nonce.  This nonce will be used when the caller requests this enclave to
 *                   generate a report.  The resulting report's report_data field will contain the SHA256(nonce||quote).
 *                   If the p_nonce is NULL, then the p_qe_report must be NULL.  IF the p_nonce is not NULL, then the
 *                   p_qe_report must not be NULL.  If provided, the buffer must reside in the enclave's memory space.
 * @param p_qe_report
 *                   [In, Out] Optional pointer to this enclave's report targeting the requesting enclave. The resulting
 *                   report's report_data field will contain the SHA256(nonce||quote).  If the p_nonce is NULL, then the
 *                   p_qe_report must be NULL.  IF the p_nonce is not NULL, then the p_qe_report must not be NULL. If
 *                   provided, the buffer must reside in the enclave's memory space.
 * @param p_quote  [out] Pointer to the output buffer for quote.
 * @param quote_size [in] The size of buffer pointed to by p_quote, in bytes.  It must be at least the value returned by
                     the ref_get_quote_size() API.
 * @param pce_isvsvn [In] The ISVSVN of the PCE currently installed on the platform.  This value will be placed in the
 *                   quote header to allow the quote verifier to know whether the PCE has been properly upgraded in case
 *                   of a TCB recover.  The verifier can then request that the attestation key to be re-certified.
 * @param p_certification_data [In] The optional cert_data, it can be NULL.
 * @param cert_data_size [in] The size of buffer pointed to by p_certification_data, in bytes.  If p_certification_data is NULL, it should
                     be 0.

 * @return REFQE3_SUCCESS
 * @return REFQE3_ERROR_INVALID_PARAMETER
 * @return REFQE3_ECDSABLOB_ERROR
 * @return REFQE3_ERROR_UNEXPECTED
 * @return REFQE3_ERROR_INVALID_REPORT
 * @return REFQE3_ERROR_OUT_OF_MEMORY
 */
uint32_t gen_quote(uint8_t *p_blob,
    uint32_t blob_size,
    const sgx_report_t *p_enclave_report,
    const sgx_quote_nonce_t *p_nonce,
    const sgx_target_info_t *p_app_enclave_target_info,
    sgx_report_t *p_qe_report_out,
    uint8_t *p_quote_buf,
    uint32_t quote_size,
    sgx_isv_svn_t pce_isvsvn,
    const uint8_t * p_certification_data,
    uint32_t cert_data_size)
{
    qe3_error_t ret = REFQE3_SUCCESS;
    sgx_quote3_t* p_quote;
    sgx_ql_ecdsa_sig_data_t *p_quote_sig;
    uint8_t is_resealed = 0;
    uint32_t sign_size = 0;
    sgx_status_t sgx_status = SGX_SUCCESS;
    sgx_report_t qe_report;
    size_t required_buffer_size = 0;
    ref_plaintext_ecdsa_data_sdk_t plaintext;
    //
    // provide extra protection for attestation key by
    // randomizing its address and securely aligning it
    //
    using cciphertext = randomly_placed_object<
        sgx::custom_alignment_aligned<
        ref_ciphertext_ecdsa_data_sdk_t,
        alignof(ref_ciphertext_ecdsa_data_sdk_t),
        __builtin_offsetof(ref_ciphertext_ecdsa_data_sdk_t, ecdsa_private_key),
        sizeof(((ref_ciphertext_ecdsa_data_sdk_t*)0)->ecdsa_private_key)>>;
    //
    // instance of randomly_placed_object
    //
    cciphertext ociphertext_buf;
    //
    // pointer to instance of custom_alignment_aligned
    //
    auto* ociphertext = ociphertext_buf.instantiate_object();
    ref_ciphertext_ecdsa_data_sdk_t* pciphertext = &ociphertext->v;

    sgx_ecc_state_handle_t handle = NULL;
    sgx_ql_auth_data_t *p_auth_data;
    sgx_ql_certification_data_t *p_certification_data_output;
#ifdef ALLOW_CLEARTEXT_PPID
    sgx_ql_ppid_cleartext_cert_info_t *p_cert_cleartext_ppid_info_data;
#endif
    sgx_ql_ppid_rsa3072_encrypted_cert_info_t *p_cert_encrypted_ppid_info_data;
    sgx_sha_state_handle_t sha_quote_context = NULL;
    sgx_report_data_t qe_report_data;
    sgx_key_128bit_t qe_id = { 0 };

    memset(&plaintext, 0, sizeof(plaintext));

    // Actually, some cases here will be checked with code generated by
    // edger8r. Here we just want to defend in depth.
    if ((NULL == p_blob) ||
        (NULL == p_enclave_report) ||
        (NULL == p_quote_buf) ||
        (!quote_size)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
    if (SGX_QL_TRUSTED_ECDSA_BLOB_SIZE_SDK != blob_size) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
    if (!(NULL == p_nonce) && (NULL == p_app_enclave_target_info) && (NULL == p_qe_report_out)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
    if (!(NULL != p_nonce) && (NULL != p_app_enclave_target_info) && (NULL != p_qe_report_out)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
    if (NULL != p_certification_data)
    {
        sgx_ql_certification_data_t * p_input_certification_data_header = (sgx_ql_certification_data_t *)p_certification_data;
        if (PPID_CLEARTEXT > p_input_certification_data_header->cert_key_type
            || QL_CERT_KEY_TYPE_MAX < p_input_certification_data_header->cert_key_type) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
        if (MAX_CERT_DATA_SIZE < p_input_certification_data_header->size) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
        if (sizeof(sgx_ql_certification_data_t) + p_input_certification_data_header->size != cert_data_size) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
    }
    if (NULL == p_certification_data && cert_data_size !=0) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }


    // The ECDSA Quote is not so large that it needs to be outside the enclave.  Verify the full buffer is within
    // the EPC.  To reduce the ECDSA QE, it can be moved outside the epc.
    if (!sgx_is_within_enclave(p_quote_buf, quote_size)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    /* Check whether p_blob is copied into EPC. If we want to reduce the
       memory usage, maybe we can leave the p_blob outside EPC. */
    if (!sgx_is_within_enclave(p_blob, blob_size)) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }
    if (!sgx_is_within_enclave(p_enclave_report, sizeof(*p_enclave_report))) {
        return(REFQE3_ERROR_INVALID_PARAMETER);
    }

    if (NULL != p_certification_data) {
        if (!sgx_is_within_enclave(p_certification_data, cert_data_size)) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
    }

    // If the code reaches here, if p_nonce is NULL, then p_qe_report will be
    // NULL also. So we only check p_nonce here.
    if (p_nonce) {
        // Actually Edger8r will alloc the buffer within EPC, this is just kind
        // of defense in depth.
        if (!sgx_is_within_enclave(p_nonce, sizeof(*p_nonce))) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
        if (!sgx_is_within_enclave(p_qe_report_out, sizeof(*p_qe_report_out))) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
        if (!sgx_is_within_enclave(p_app_enclave_target_info, sizeof(*p_app_enclave_target_info))) {
            return(REFQE3_ERROR_INVALID_PARAMETER);
        }
    }

    // Verify the input report.
    if (SGX_SUCCESS != sgx_verify_report(p_enclave_report)) {
        return(REFQE3_ERROR_INVALID_REPORT);
    }

    // Verify EPID p_blob and create the context
    ret = random_stack_advance(verify_blob_internal,p_blob,
        blob_size,
        &is_resealed,
        &plaintext,
        (sgx_report_body_t*) NULL,
        (uint8_t*) NULL,
        0,
        pciphertext);
    if (REFQE3_SUCCESS != ret) {
        goto ret_point;
    }

    sign_size = sizeof(sgx_ql_ecdsa_sig_data_t) +           // ECDSA sig data structure
        sizeof(sgx_ql_auth_data_t) +
        sizeof(sgx_ql_certification_data_t);
    if (1 == pciphertext->is_clear_ppid) {
        sign_size += (uint32_t)sizeof(sgx_ql_ppid_cleartext_cert_info_t);  // PPID, PCE PSVN and PCE_ID
    }
    else {
        if (!p_certification_data) {
            sign_size += (uint32_t)sizeof(sgx_ql_ppid_rsa3072_encrypted_cert_info_t);  // RSA3072_Enc_PPID, PCE PSVN and PCE_ID
        }
        else {
            sign_size += ((sgx_ql_certification_data_t *)p_certification_data)->size;
        }
    }
    /* Check for overflow before adding in the variable size of authentication data. */
    if ((UINT32_MAX - sign_size - sizeof(sgx_quote3_t)) < plaintext.authentication_data_size) {
        ret = REFQE3_ERROR_INVALID_PARAMETER;
        goto ret_point;
    }
    sign_size += plaintext.authentication_data_size;     // Authentication data

    required_buffer_size = sizeof(sgx_quote3_t) + sign_size;

    // Make sure the buffer size is big enough.
    if (quote_size < required_buffer_size) {
        ret = REFQE3_ERROR_INVALID_PARAMETER;
        goto ret_point;
    }

    // Verify sizeof header.userdata is large enough
    ref_static_assert(sizeof(qe_id) <= sizeof(p_quote->header.user_data));

    // Clear out the quote buffer
    sgx_lfence();
    memset(p_quote_buf, 0, required_buffer_size);
    // Set up the component quote structure pointers to point to the correct place within the inputted quote buffer.
    p_quote = (sgx_quote3_t*)p_quote_buf;
    p_quote->signature_data_len = sign_size;
    p_quote_sig = (sgx_ql_ecdsa_sig_data_t*)(p_quote->signature_data);
    p_auth_data = (sgx_ql_auth_data_t*)(p_quote_sig->auth_certification_data);
    p_auth_data->size = (uint16_t)plaintext.authentication_data_size;
    //Note:  This is potentially dangerous pointer math using an untrusted input size.  The 'required_buffer_size' check
    //above verifies that the size will not put the calculated address and certification data outside of the inputted
    //p_quote + quote_size memory.
    p_certification_data_output = (sgx_ql_certification_data_t*)((uint8_t*)p_auth_data + sizeof(*p_auth_data) + p_auth_data->size);

    // Populate the quote buffer.
    // Set up the header.
    p_quote->header.version = QE_QUOTE_VERSION;
    p_quote->header.att_key_type = SGX_QL_ALG_ECDSA_P256;
    p_quote->header.pce_svn = pce_isvsvn; // Both are little endian
    // Sizes of user_data and qe_id were checked above.  If here, then sizes are OK without overflow.
    ///todo:  Verify that the QE_ID  matches the value in the blob.
    ret = get_qe_id_internal(&qe_id);
    if (REFQE3_SUCCESS != ret) {
        goto ret_point;
    }
    memcpy(&p_quote->header.user_data, &qe_id, sizeof(qe_id));
    // Copy in Intel's Vender ID
    memcpy(p_quote->header.vendor_id, g_vendor_id, 16);
    //  Copy the incoming report into Quote body.
    memcpy(&p_quote->report_body, &(p_enclave_report->body), sizeof(p_quote->report_body));

    memset(&qe_report_data, 0, sizeof(qe_report_data));
    sgx_status = sgx_create_report(NULL, &qe_report_data, &qe_report);
    if (SGX_SUCCESS != sgx_status) {
        if (SGX_ERROR_OUT_OF_MEMORY == sgx_status) {
            ret = REFQE3_ERROR_OUT_OF_MEMORY;
        }
        else {
            ret = REFQE3_ERROR_UNEXPECTED;
        }
        goto ret_point;
    }
    // Copy QE's security version in to Quote header.
    p_quote->header.qe_svn = qe_report.body.isv_svn;

    // Generate the quote signature.
    sgx_status = sgx_ecc256_open_context(&handle);
    if (SGX_ERROR_OUT_OF_MEMORY == sgx_status) {
        ret = REFQE3_ERROR_OUT_OF_MEMORY;
        goto ret_point;
    }
    else if (SGX_SUCCESS != sgx_status) {
        ret = (qe3_error_t)sgx_status;
        goto ret_point;
    }
    // Sign everything in the quote except the signature_data_len.  This allows the quote certification information to change
    // to contain the actual PCK cert after initially only carring the PPID+PCEID+TCB without making the quote invalid.
    sgx_status = sgx_ecdsa_sign(reinterpret_cast<const uint8_t *>(p_quote),
        sizeof(*p_quote) - sizeof(p_quote->signature_data_len),
        &pciphertext->ecdsa_private_key,
        reinterpret_cast<sgx_ec256_signature_t *>(p_quote_sig->sig),
        handle);
    if (SGX_ERROR_OUT_OF_MEMORY == sgx_status) {
        ret = REFQE3_ERROR_OUT_OF_MEMORY;
        goto ret_point;
    }
    else if (SGX_SUCCESS != sgx_status) {
        ret = REFQE3_ERROR_UNEXPECTED;
        goto ret_point;
    }
    // Swap signature x and y from little endian used in sgx_crypto to big endian used in quote byte order
    {
        size_t i;
        uint8_t swap;
        for (i = 0; i < 32 / 2; i++) {
            swap = p_quote_sig->sig[i];
            p_quote_sig->sig[i] = p_quote_sig->sig[32 - 1 - i];
            p_quote_sig->sig[32 - 1 - i] = swap;
        }
        for (i = 0; i < 32 / 2; i++) {
            swap = p_quote_sig->sig[32 + i];
            p_quote_sig->sig[32 + i] = p_quote_sig->sig[64 - 1 - i];
            p_quote_sig->sig[64 - 1 - i] = swap;
        }
    }
    // Add the public part of the ECDSA key to the quote sig data.  Store it in Big Endian
    memcpy(p_quote_sig->attest_pub_key, &plaintext.ecdsa_att_public_key, sizeof(p_quote_sig->attest_pub_key));

    // Add the QE Report to the Quote sig data (the qe report when it was signed by the PCE!).
    memcpy(&p_quote_sig->qe3_report, &plaintext.qe3_report.body, sizeof(p_quote_sig->qe3_report));

    // Add the PCE signature
    memcpy(p_quote_sig->qe3_report_sig, &plaintext.qe3_report_cert_key_sig, sizeof(p_quote_sig->qe3_report_sig));

    // Copy in the Authentication Data
    if (0 != p_auth_data->size) {
        memcpy(p_auth_data->auth_data, plaintext.authentication_data, p_auth_data->size);
    }

    if (1 == pciphertext->is_clear_ppid) {
#ifdef ALLOW_CLEARTEXT_PPID
        p_cert_cleartext_ppid_info_data = (sgx_ql_ppid_cleartext_cert_info_t *)p_certification_data_output->certification_data;
        // Prepare the the certificaiton data.  PPID_CLEARTEXT = Plaintext PPID + PCE_TCB + PCEID is supported by the referecne.
        p_certification_data_output->cert_key_type = PPID_CLEARTEXT;
        p_certification_data_output->size = sizeof(sgx_ql_ppid_cleartext_cert_info_t);
        // Get the cert_info_data from the ECDSA blob.
        memcpy(p_cert_cleartext_ppid_info_data->ppid, pciphertext->ppid, sizeof(p_cert_cleartext_ppid_info_data->ppid));
#ifdef USE_PCEID
        p_cert_cleartext_ppid_info_data->pce_info = plaintext.cert_pce_info;
#else
        p_cert_cleartext_ppid_info_data->pce_info.pce_isv_svn = plaintext.cert_pce_info.pce_isv_svn;
#endif
        memcpy(&p_cert_cleartext_ppid_info_data->cpu_svn, &plaintext.cert_cpu_svn, sizeof(p_cert_cleartext_ppid_info_data->cpu_svn));
#else
        ret = REFQE3_ERROR_UNEXPECTED;
        goto ret_point;
#endif
    }
    else {
        if (NULL == p_certification_data) {
            p_cert_encrypted_ppid_info_data = (sgx_ql_ppid_rsa3072_encrypted_cert_info_t *)p_certification_data_output->certification_data;
            // Prepare the the certificaiton data.  PPID_RSA3072_ENCRYPTED = Encrypted_PPID + PCE_TCB + PCEID is supported by the referecne.
            p_certification_data_output->cert_key_type = PPID_RSA3072_ENCRYPTED;
            p_certification_data_output->size = sizeof(sgx_ql_ppid_rsa3072_encrypted_cert_info_t);
            // Get the cert_info_data from the ECDSA blob.
            memcpy(p_cert_encrypted_ppid_info_data->enc_ppid, pciphertext->encrypted_ppid_data.encrypted_ppid, sizeof(p_cert_encrypted_ppid_info_data->enc_ppid));
#ifdef USE_PCEID
            p_cert_encrypted_ppid_info_data->pce_info = plaintext.cert_pce_info;
#else
            p_cert_encrypted_ppid_info_data->pce_info.pce_isv_svn = plaintext.cert_pce_info.pce_isv_svn;
#endif
            memcpy(&p_cert_encrypted_ppid_info_data->cpu_svn, &plaintext.cert_cpu_svn, sizeof(p_cert_encrypted_ppid_info_data->cpu_svn));
        }
        else {
            sgx_ql_certification_data_t * p_input_certification_data_header = (sgx_ql_certification_data_t *)p_certification_data;
            p_certification_data_output->cert_key_type = p_input_certification_data_header->cert_key_type;
            p_certification_data_output->size = p_input_certification_data_header->size;
            // Get the cert_info_data from the ECDSA blob.
            memcpy(p_certification_data_output->certification_data, &p_input_certification_data_header->certification_data, p_input_certification_data_header->size);
        }
    }

    // Get the QE's report if requested.
    ///todo:  It is possible that the untrusted code can change the certification data of the quote (including the
    //the signature_length.  We may need to modify the quote hash generation to skip the modifiable values!!
    if (NULL != p_nonce) {
        ref_static_assert(sizeof(qe_report_data) >= sizeof(sgx_sha256_hash_t));

        sgx_status = sgx_sha256_init(&sha_quote_context);
        if (SGX_SUCCESS != sgx_status) {
            ret = REFQE3_ERROR_UNEXPECTED;
            goto ret_point;
        }

        memset(&qe_report_data, 0, sizeof(qe_report_data));
        // Update hash for nonce.
        sgx_status = sgx_sha256_update((uint8_t *)const_cast<sgx_quote_nonce_t *>(p_nonce),
            (uint32_t)sizeof(*p_nonce),
            sha_quote_context);
        if (SGX_SUCCESS != sgx_status) {
            ret = REFQE3_ERROR_UNEXPECTED;
            goto ret_point;
        }
        // Update hash with the quote.
        sgx_status = sgx_sha256_update(p_quote_buf,
            (uint32_t)required_buffer_size,
            sha_quote_context);
        if (SGX_SUCCESS != sgx_status) {
            ret = REFQE3_ERROR_UNEXPECTED;
            goto ret_point;
        }
        sgx_status = sgx_sha256_get_hash(sha_quote_context,
            (sgx_sha256_hash_t *)&qe_report_data);
        if (SGX_SUCCESS != sgx_status) {
            ret = REFQE3_ERROR_UNEXPECTED;
            goto ret_point;
        }
        ///todo:  Evaluate the requirements on the format of target_info structure.
        sgx_status = sgx_create_report(p_app_enclave_target_info, &qe_report_data, &qe_report);
        if (SGX_SUCCESS != sgx_status) {
            if (SGX_ERROR_OUT_OF_MEMORY == sgx_status) {
                ret = REFQE3_ERROR_OUT_OF_MEMORY;
            }
            else {
                ret = REFQE3_UNABLE_TO_GENERATE_QE_REPORT;
            }
            goto ret_point;
        }
        if (NULL != p_qe_report_out) {
            memcpy(p_qe_report_out, &qe_report, sizeof(*p_qe_report_out));
        }
    }

ret_point:
    // Clear out any senstive data.
    memset_s(pciphertext, sizeof(*pciphertext), 0, sizeof(*pciphertext));
    if (handle != NULL) {
        sgx_ecc256_close_context(handle);
    }
    if (sha_quote_context != NULL) {
        sgx_sha256_close(sha_quote_context);
    }

    return(ret);
}

