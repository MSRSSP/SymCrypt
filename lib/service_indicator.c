//
// service_indicator.c Implements FIPS 140-3 Approved Services Indicator
//
// Copyright (c) Microsoft Corporation. Licensed under the MIT license.
//


#include "precomp.h"

#define SYMCRYPT_SI_HAS_WEIGHT_ONE(X)   (((X) != 0) && (((X) & ((X) - 1)) == 0))

// Integer greater than or equal
#define SYMCRYPT_SI_INTGE(X)            SYMCRYPT_SI_INTRANGE((X), SYMCRYPT_SI_INTMASK)

#define SYMCRYPT_SI_TYPE_BITS(X)        ((X) >> 56)
#define SYMCRYPT_SI_DATA_BITS(X)        ((X) & ((1ULL << 56) - 1))
#define SYMCRYPT_SI_IS_TYPE_ALG(X)      ((SYMCRYPT_SI_TYPE_BITS(X) > 0x00) && (SYMCRYPT_SI_TYPE_BITS(X) < 0x40))
#define SYMCRYPT_SI_IS_TYPE_BITMASK(X)  ((SYMCRYPT_SI_TYPE_BITS(X) > 0x00) && (SYMCRYPT_SI_TYPE_BITS(X) < 0x80))

#define SYMCRYPT_SI_PARAM_NONE          0, 0
#define SYMCRYPT_SI_PARAM1(X)           (X), 0
#define SYMCRYPT_SI_PARAM2(X, Y)        (X), (Y)

typedef struct _SYMCRYPT_FIPS_ALG_DATA
{
    UINT64 Alg;
    UINT64 Param1;
    UINT64 Param2;
} SYMCRYPT_SI_ALG_DATA;

typedef struct _SYMCRYPT_SI_ALGMAP
{
    SYMCRYPT_SI_ALG_DATA*   pAlgData;
    UINT32                  nItems;
    UINT32                  Service;
} SYMCRYPT_SI_ALGMAP;

static SYMCRYPT_SI_ALG_DATA _Cipher[] = {
    {
        SYMCRYPT_SI_AES_CBC | SYMCRYPT_SI_AES_CCM | SYMCRYPT_SI_AES_CFB128 | SYMCRYPT_SI_AES_CFB8 | \
        SYMCRYPT_SI_AES_CTR | SYMCRYPT_SI_AES_ECB | SYMCRYPT_SI_AES_GCM,
        SYMCRYPT_SI_PARAM_NONE
    },
    {
        SYMCRYPT_SI_AES_XTS,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_KEYBITS(128)
        )
    },
    {
        SYMCRYPT_SI_AES_XTS,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_KEYBITS(256)
        )
    },
};

static SYMCRYPT_SI_ALG_DATA _Hash[] = {
    {
        SYMCRYPT_SI_SHA1                                                    | \
        SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_384 | SYMCRYPT_SI_SHA2_512  | \
        SYMCRYPT_SI_SHA3_256 | SYMCRYPT_SI_SHA3_384 | SYMCRYPT_SI_SHA3_512  | \
        SYMCRYPT_SI_SHAKE128 | SYMCRYPT_SI_SHAKE256 | SYMCRYPT_SI_CSHAKE128 | SYMCRYPT_SI_CSHAKE256,
        SYMCRYPT_SI_PARAM_NONE
    },
};

static SYMCRYPT_SI_ALG_DATA _Mac[] = {
    {
        SYMCRYPT_SI_HMAC_SHA1                                                               | \
        SYMCRYPT_SI_HMAC_SHA2_256 | SYMCRYPT_SI_HMAC_SHA2_384 | SYMCRYPT_SI_HMAC_SHA2_512   | \
        SYMCRYPT_SI_HMAC_SHA3_256 | SYMCRYPT_SI_HMAC_SHA3_384 | SYMCRYPT_SI_HMAC_SHA3_512,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_INTGE(112)
        )
    },

    {
        SYMCRYPT_SI_AES_CMAC | SYMCRYPT_SI_AES_GMAC | SYMCRYPT_SI_KMAC128 | SYMCRYPT_SI_KMAC256,
        SYMCRYPT_SI_PARAM_NONE
    },
};

static SYMCRYPT_SI_ALG_DATA _Kdf[] = {
    {
        SYMCRYPT_SI_KDA_ONESTEP, 
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_SHA1                                                    | \
            SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_384 | SYMCRYPT_SI_SHA2_512  | \
            SYMCRYPT_SI_SHA3_256 | SYMCRYPT_SI_SHA3_384 | SYMCRYPT_SI_SHA3_512
        )
    },

    {
        SYMCRYPT_SI_KDA_ONESTEP,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_HMAC_SHA1                                                               | \
            SYMCRYPT_SI_HMAC_SHA2_256 | SYMCRYPT_SI_HMAC_SHA2_384 | SYMCRYPT_SI_HMAC_SHA2_512   | \
            SYMCRYPT_SI_HMAC_SHA3_256 | SYMCRYPT_SI_HMAC_SHA3_384 | SYMCRYPT_SI_HMAC_SHA3_512   | \
            SYMCRYPT_SI_KMAC128 | SYMCRYPT_SI_KMAC256
        )
    },

    {
        SYMCRYPT_SI_KDF_SP800_108_CTR,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_AES_CMAC                                                                | \
            SYMCRYPT_SI_HMAC_SHA1                                                               | \
            SYMCRYPT_SI_HMAC_SHA2_256 | SYMCRYPT_SI_HMAC_SHA2_384 | SYMCRYPT_SI_HMAC_SHA2_512   | \
            SYMCRYPT_SI_HMAC_SHA3_256 | SYMCRYPT_SI_HMAC_SHA3_384 | SYMCRYPT_SI_HMAC_SHA3_512
        )
    },

    {
        SYMCRYPT_SI_KDF_SRTP | SYMCRYPT_SI_KDF_TLS,
        SYMCRYPT_SI_PARAM_NONE
    },

    {
        SYMCRYPT_SI_KDF_SSH,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_SHA1 | SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_384 | SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_PBKDF,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_HMAC_SHA1                                                               | \
            SYMCRYPT_SI_HMAC_SHA2_256 | SYMCRYPT_SI_HMAC_SHA2_384 | SYMCRYPT_SI_HMAC_SHA2_512   | \
            SYMCRYPT_SI_HMAC_SHA3_256 | SYMCRYPT_SI_HMAC_SHA3_384 | SYMCRYPT_SI_HMAC_SHA3_512
        )
    },

    {
        SYMCRYPT_SI_KDF_TLS_V12,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_384
        )
    },
};

static SYMCRYPT_SI_ALG_DATA _Drbg[] = {
    SYMCRYPT_SI_CTR_DRBG_AES256, SYMCRYPT_SI_PARAM_NONE
};

static SYMCRYPT_SI_ALG_DATA _KeyGen[] = {
    {
        SYMCRYPT_SI_ECDSA_KEYGEN,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_ECURVE_NISTP256 | SYMCRYPT_SI_ECURVE_NISTP384 | SYMCRYPT_SI_ECURVE_NISTP521
        )
    },

    {
        SYMCRYPT_SI_RSA_KEYGEN,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_MODULUS(2048)
        )
    },

    {
        SYMCRYPT_SI_RSA_KEYGEN,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_MODULUS(3072)
        )
    },

    {
        SYMCRYPT_SI_RSA_KEYGEN,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_MODULUS(4096)
        )
    },

    {
        SYMCRYPT_SI_SAFE_PRIME_KEYGEN,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_SPG_FFDHE_2048 | SYMCRYPT_SI_SPG_FFDHE_3072 | SYMCRYPT_SI_SPG_FFDHE_4096 | SYMCRYPT_SI_SPG_FFDHE_6144 | \
            SYMCRYPT_SI_SPG_MODP_2048 | SYMCRYPT_SI_SPG_MODP_3072 | SYMCRYPT_SI_SPG_MODP_4096 | SYMCRYPT_SI_SPG_MODP_6144
        )
    },
};

static SYMCRYPT_SI_ALG_DATA _KeyVer[] = {

    {
        SYMCRYPT_SI_DSA_PQGVER,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_DSAPARAMS(2048, 256)
        )
    },

    {
        SYMCRYPT_SI_DSA_PQGVER,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_DSAPARAMS(3072, 256)
        )
    },

    {
        SYMCRYPT_SI_ECDSA_KEYVER,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_ECURVE_NISTP256 | SYMCRYPT_SI_ECURVE_NISTP384 | SYMCRYPT_SI_ECURVE_NISTP521
        )
    },
};

static SYMCRYPT_SI_ALG_DATA _SigGen[] = {
    {
        SYMCRYPT_SI_ECDSA_SIGGEN | SYMCRYPT_SI_ECDSA_SIGGEN_COMP,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_ECURVE_NISTP256, SYMCRYPT_SI_SHA2_256
        )
    },

    {
        SYMCRYPT_SI_ECDSA_SIGGEN | SYMCRYPT_SI_ECDSA_SIGGEN_COMP,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_ECURVE_NISTP384, SYMCRYPT_SI_SHA2_384
        )
    },

    {
        SYMCRYPT_SI_ECDSA_SIGGEN | SYMCRYPT_SI_ECDSA_SIGGEN_COMP,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_ECURVE_NISTP521, SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_RSA_SIGGEN_PKCS15 | SYMCRYPT_SI_RSA_SIGGEN_PKCSPSS,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_MODULUS(2048),
            SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_384 | SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_RSA_SIGGEN_PKCS15 | SYMCRYPT_SI_RSA_SIGGEN_PKCSPSS,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_MODULUS(3072),
            SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_384 | SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_RSA_SIGGEN_PKCS15 | SYMCRYPT_SI_RSA_SIGGEN_PKCSPSS,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_MODULUS(4096),
            SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_384 | SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_RSA_SIG_PRIM,
        SYMCRYPT_SI_PARAM_NONE
    },
};

static SYMCRYPT_SI_ALG_DATA _SigVer[] = {
    {
        SYMCRYPT_SI_DSA_SIGVER,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_DSAPARAMS(2048, 256), SYMCRYPT_SI_SHA2_256
        )
    },

    {
        SYMCRYPT_SI_DSA_SIGVER,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_DSAPARAMS(3072, 256), SYMCRYPT_SI_SHA2_256
        )
    },

    {
        SYMCRYPT_SI_ECDSA_SIGVER,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_ECURVE_NISTP256, SYMCRYPT_SI_SHA2_256
        )
    },

    {
        SYMCRYPT_SI_ECDSA_SIGVER,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_ECURVE_NISTP384, SYMCRYPT_SI_SHA2_384
        )
    },

    {
        SYMCRYPT_SI_ECDSA_SIGVER,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_ECURVE_NISTP521, SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_RSA_SIGVER_PKCS15 | SYMCRYPT_SI_RSA_SIGVER_PKCSPSS,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_MODULUS(1024),
            SYMCRYPT_SI_SHA1 | SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_384 | SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_RSA_SIGVER_PKCS15 | SYMCRYPT_SI_RSA_SIGVER_PKCSPSS,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_MODULUS(2048),
            SYMCRYPT_SI_SHA1 | SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_384 | SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_RSA_SIGVER_PKCS15 | SYMCRYPT_SI_RSA_SIGVER_PKCSPSS,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_MODULUS(3072),
            SYMCRYPT_SI_SHA1 | SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_384 | SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_RSA_SIGVER_PKCS15 | SYMCRYPT_SI_RSA_SIGVER_PKCSPSS,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_MODULUS(4096),
            SYMCRYPT_SI_SHA1 | SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_384 | SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_RSA_SIG_PRIM,
        SYMCRYPT_SI_PARAM_NONE
    },
};

static SYMCRYPT_SI_ALG_DATA _Kas[] = {
    {
        SYMCRYPT_SI_KAS_ECC,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_ECURVE_NISTP256, SYMCRYPT_SI_SHA2_256
        )
    },

    {
        SYMCRYPT_SI_KAS_ECC,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_ECURVE_NISTP384, SYMCRYPT_SI_SHA2_384
        )
    },

    {
        SYMCRYPT_SI_KAS_ECC,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_ECURVE_NISTP521, SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_KAS_ECC_SSC,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_ECURVE_NISTP256 | SYMCRYPT_SI_ECURVE_NISTP384 | SYMCRYPT_SI_ECURVE_NISTP521,
            SYMCRYPT_SI_SCHEME_EPHEM_UNIFIED
        )
    },

    {
        SYMCRYPT_SI_KAS_FFC,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_SPG_FFDHE_2048 | SYMCRYPT_SI_SPG_FFDHE_3072 | SYMCRYPT_SI_SPG_FFDHE_4096 | SYMCRYPT_SI_SPG_FFDHE_6144 | \
            SYMCRYPT_SI_SPG_MODP_2048 | SYMCRYPT_SI_SPG_MODP_3072 | SYMCRYPT_SI_SPG_MODP_4096 | SYMCRYPT_SI_SPG_MODP_6144,
            SYMCRYPT_SI_SHA2_256 | SYMCRYPT_SI_SHA2_512
        )
    },

    {
        SYMCRYPT_SI_KAS_FFC_SSC,
        SYMCRYPT_SI_PARAM2(
            SYMCRYPT_SI_SPG_FFDHE_2048 | SYMCRYPT_SI_SPG_FFDHE_3072 | SYMCRYPT_SI_SPG_FFDHE_4096 | SYMCRYPT_SI_SPG_FFDHE_6144 | \
            SYMCRYPT_SI_SPG_MODP_2048 | SYMCRYPT_SI_SPG_MODP_3072 | SYMCRYPT_SI_SPG_MODP_4096 | SYMCRYPT_SI_SPG_MODP_6144,
            SYMCRYPT_SI_SCHEME_DH_EPHEM | SYMCRYPT_SI_SCHEME_DH_ONEFLOW | SYMCRYPT_SI_SCHEME_DH_STATIC
        )
    },
};

static SYMCRYPT_SI_ALG_DATA _Dec[] = {
    {
        SYMCRYPT_SI_RSA_DEC_PRIM,
        SYMCRYPT_SI_PARAM1(
            SYMCRYPT_SI_MODULUS(2048)
        )
    },
};

static SYMCRYPT_SI_ALGMAP _ServiceIndicatorData[] = {

    // Symmetric Ciphers
    {         
        _Cipher, SYMCRYPT_ARRAY_SIZE(_Cipher),
        SYMCRYPT_SI_SVC_ENCRYPTION | SYMCRYPT_SI_SVC_DECRYPTION
    },

    // Hash functions
    {
        _Hash, SYMCRYPT_ARRAY_SIZE(_Hash),
        SYMCRYPT_SI_SVC_HASHING
    },

    // MACs
    {
        _Mac, SYMCRYPT_ARRAY_SIZE(_Mac),
        SYMCRYPT_SI_SVC_MESSAGE_AUTHENTICATION
    },

    // KDF
    {
        _Kdf, SYMCRYPT_ARRAY_SIZE(_Kdf),
        SYMCRYPT_SI_SVC_KEY_DERIVATION
    },

    // DRBG
    {
        _Drbg, SYMCRYPT_ARRAY_SIZE(_Drbg),
        SYMCRYPT_SI_SVC_RANDOM_NUMBER_GENERATION
    },

    // Asymmetric Key Generation
    {
        _KeyGen, SYMCRYPT_ARRAY_SIZE(_KeyGen),
        SYMCRYPT_SI_SVC_ASYMMETRIC_KEY_GENERATION
    },

    // Asymmetric Key Verification
    {
        _KeyVer, SYMCRYPT_ARRAY_SIZE(_KeyVer),
        SYMCRYPT_SI_SVC_ASYMMETRIC_KEY_VERIFICATION
    },

    // Signature Generation
    {
        _SigGen, SYMCRYPT_ARRAY_SIZE(_SigGen),
        SYMCRYPT_SI_SVC_SIGNATURE_GENERATION
    },

    // Signature Verification
    {
        _SigVer, SYMCRYPT_ARRAY_SIZE(_SigVer),
        SYMCRYPT_SI_SVC_SIGNATURE_VERIFICATION
    },

    // Secret Agreement
    {
        _Kas, SYMCRYPT_ARRAY_SIZE(_Kas),
        SYMCRYPT_SI_SVC_SECRET_AGREEMENT
    },

    // Decryption
    {
        _Dec, SYMCRYPT_ARRAY_SIZE(_Dec),
        SYMCRYPT_SI_SVC_DECRYPTION
    },
};

static
BOOLEAN
SYMCRYPT_CALL
ServiceIndicatorValidateAlgorithm(UINT64 Alg)
{
    UINT64 dataAlg = SYMCRYPT_SI_DATA_BITS(Alg);

    return (SYMCRYPT_SI_IS_TYPE_ALG(Alg) && 
            SYMCRYPT_SI_HAS_WEIGHT_ONE(dataAlg));
}

static
BOOLEAN
SYMCRYPT_CALL
ServiceIndicatorValidateParameter(UINT64 Param, UINT8 Type)
{
    BYTE typeParam = SYMCRYPT_SI_TYPE_BITS(Param);
    UINT64 dataParam = SYMCRYPT_SI_DATA_BITS(Param);

    return  (typeParam == Type && 
            SYMCRYPT_SI_HAS_WEIGHT_ONE(dataParam));
}

static
BOOLEAN
SYMCRYPT_CALL
ServiceIndicatorIsAlgorithmContained(UINT64 Input, UINT64 Rule)
{
    BYTE typeInput = SYMCRYPT_SI_TYPE_BITS(Input);
    BYTE typeRule = SYMCRYPT_SI_TYPE_BITS(Rule);

    UINT64 dataInput = SYMCRYPT_SI_DATA_BITS(Input);
    UINT64 dataRule = SYMCRYPT_SI_DATA_BITS(Rule);
 
    SYMCRYPT_ASSERT(SYMCRYPT_SI_IS_TYPE_ALG(Rule));
    SYMCRYPT_ASSERT(dataRule != 0);

    return (ServiceIndicatorValidateAlgorithm(Input) &&
        (typeInput == typeRule) &&
        ((dataInput & dataRule) == dataInput));
}

static
BOOLEAN
SYMCRYPT_CALL
ServiceIndicatorIsParameterContained(UINT64 Input, UINT64 Rule)
{
    BYTE typeRule = SYMCRYPT_SI_TYPE_BITS(Rule);
    UINT64 dataInput = SYMCRYPT_SI_DATA_BITS(Input);
    UINT64 dataRule = SYMCRYPT_SI_DATA_BITS(Rule);

    SYMCRYPT_ASSERT(typeRule != 0);
    SYMCRYPT_ASSERT(dataRule != 0);

    return (ServiceIndicatorValidateParameter(Input, typeRule) &&
        ((dataInput & dataRule) == dataInput));
}

static
BOOLEAN
SYMCRYPT_CALL
ServiceIndicatorIsParamApproved(UINT64 Input, UINT64 Rule)
{
    BYTE result = FALSE;
    BYTE typeRule = (BYTE)SYMCRYPT_SI_TYPE_BITS(Rule);
    BYTE typeInput = (BYTE)SYMCRYPT_SI_TYPE_BITS(Input);

    // Types must always match
    if (typeInput != typeRule)
    {
        return FALSE;
    }

    if (SYMCRYPT_SI_IS_TYPE_BITMASK(Rule))
    {
        // We expect the input to be a single bit set from a bitmask
        // of type specified by the rule.
        result = ServiceIndicatorIsParameterContained(Input, Rule);
    }
    else if (typeRule == SYMCRYPT_SI_TYPE_INTRANGE)
    {
        // If the rule is an integer range, the input must be contained in the range.
        UINT64 uInputLow = SYMCRYPT_SI_INTUNPACKLO(Input);
        UINT64 uInputHigh = SYMCRYPT_SI_INTUNPACKHI(Input);
        UINT64 uRuleLow = SYMCRYPT_SI_INTUNPACKLO(Rule);
        UINT64 uRuleHigh = SYMCRYPT_SI_INTUNPACKHI(Rule);

        // Only allow integers that are multiples of 8
        if ((uInputLow % 8 != 0) || (uInputHigh % 8 != 0))
        {
            return FALSE;
        }

        result = (uInputLow >= uRuleLow && uInputHigh <= uRuleHigh);
    }
    else
    {
        // covers the NULL parameter case
        result = (Input == Rule);
    }

    return result;
}

UINT32
SYMCRYPT_CALL
SymCryptDeprecatedServiceIndicator(
    UINT32 Service,
    UINT64 Alg,
    UINT64 Param1,
    UINT64 Param2,
    UINT64 Param3)
{
    UNREFERENCED_PARAMETER(Param3);

    // Only one service must be provided
    if (!SYMCRYPT_SI_HAS_WEIGHT_ONE(Service))
    {
        goto cleanup;
    }

    // Algorithm has to be of correct type and the remaining part (data bits)
    // has exactly one bit set
    if (!ServiceIndicatorValidateAlgorithm(Alg))
    {
        goto cleanup;
    }

    // Search the service indicator data to find whether the given algorithm with
    // the given parameters if any, appear as part of a service
    for (UINT32 i = 0; i < SYMCRYPT_ARRAY_SIZE(_ServiceIndicatorData); i++)
    {
        // Service mask has to be a subset of the defined service data
        if ((Service & _ServiceIndicatorData[i].Service) == Service)
        {
            for (UINT32 j = 0; j < _ServiceIndicatorData[i].nItems; j++)
            {
                SYMCRYPT_SI_ALG_DATA* pAlgData = &_ServiceIndicatorData[i].pAlgData[j];
                
                if (ServiceIndicatorIsAlgorithmContained(Alg, pAlgData->Alg))
                {
                    // Found a matching entry for the queried algorithm, validate
                    // the parameters
                    if (ServiceIndicatorIsParamApproved(Param1, pAlgData->Param1) &&
                        ServiceIndicatorIsParamApproved(Param2, pAlgData->Param2))
                    {
                        // approved status
                        return 0;
                    }
                }
            }
        }
    }

cleanup:

    // non-approved status
    return 1;
}