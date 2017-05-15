#ifndef _WINSTORE_WINCRYPT_H
#define _WINSTORE_WINCRYPT_H

#if defined(__clang__) && __has_include_next(<wincrypt.h>)
#  include_next <wincrypt.h>
#else /* no wincrypt.h */

#include <windows.h>
#include <winapifamily.h>

#ifndef WINCRYPT32API 
#define WINCRYPT32API DECLSPEC_IMPORT
#endif

#if (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP || WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)

# define COBJMACROS
# define INITGUID
# include <winstring.h>
# include <windows.security.cryptography.h>
# include <windows.security.cryptography.core.h>
# ifndef _MSC_VER /* roapi.h is a C++ include file that doesn't work with C files */
#  include <roapi.h>
# else /* _MSC_VER */
DECLSPEC_IMPORT HRESULT WINAPI RoGetActivationFactory(HSTRING activatableClassId, REFIID iid, void **factory);
typedef __x_ABI_CWindows_CSecurity_CCryptography_CICryptographicBufferStatics ICryptographicBufferStatics;
typedef __x_ABI_CWindows_CStorage_CStreams_CIBuffer                           IBuffer;
#   define ICryptographicBufferStatics_Release         __x_ABI_CWindows_CSecurity_CCryptography_CICryptographicBufferStatics_Release
#   define ICryptographicBufferStatics_CopyToByteArray __x_ABI_CWindows_CSecurity_CCryptography_CICryptographicBufferStatics_CopyToByteArray
#   define IBuffer_Release                             __x_ABI_CWindows_CStorage_CStreams_CIBuffer_Release
#   define ICryptographicBufferStatics_GenerateRandom  __x_ABI_CWindows_CSecurity_CCryptography_CICryptographicBufferStatics_GenerateRandom
DEFINE_GUID(IID_ICryptographicBufferStatics, 0x320b7e22, 0x3cb0, 0x4cdf, 0x86,0x63, 0x1d,0x28,0x91,0x00,0x65,0xeb);

typedef __x_ABI_CWindows_CSecurity_CCryptography_CCertificates_CICertificateStore         ICertificateStore;
typedef __x_ABI_CWindows_CSecurity_CCryptography_CCertificates_CICertificateStoresStatics ICertificateStoresStatics;
#define      ICertificateStoresStatics_Release                                  __x_ABI_CWindows_CSecurity_CCryptography_CCertificates_CICertificateStoresStatics_Release
#define      ICertificateStoresStatics_get_TrustedRootCertificationAuthorities  __x_ABI_CWindows_CSecurity_CCryptography_CCertificates_CICertificateStoresStatics_get_TrustedRootCertificationAuthorities
#define      ICertificateStoresStatics_get_IntermediateCertificationAuthorities __x_ABI_CWindows_CSecurity_CCryptography_CCertificates_CICertificateStoresStatics_get_IntermediateCertificationAuthorities
DEFINE_GUID(IID_ICertificateStoresStatics,   0xFBECC739, 0xC6FE, 0x4DE7, 0x99, 0xCF, 0x74, 0xC3, 0xE5, 0x96, 0xE0, 0x32);
# endif /* _MSC_VER */

#define HCRYPTPROV_DEFINED
typedef void *HCRYPTPROV_LEGACY;
typedef ICertificateStore *HCERTSTORE;

typedef struct {
    DWORD dwCertEncodingType;
    BYTE *pbCertEncoded;
    DWORD cbCertEncoded;
} CERT_CONTEXT;
typedef const CERT_CONTEXT *PCCERT_CONTEXT;
typedef struct {
    DWORD dwCertEncodingType;
    BYTE *pbCrlEncoded;
    DWORD cbCrlEncoded;
} CRL_CONTEXT;
typedef const CRL_CONTEXT *PCCRL_CONTEXT;
typedef struct _CRYPTOAPI_BLOB {
    DWORD cbData;
    BYTE  *pbData;
} CRYPT_INTEGER_BLOB, *PCRYPT_INTEGER_BLOB,
CRYPT_UINT_BLOB, *PCRYPT_UINT_BLOB,
CRYPT_OBJID_BLOB, *PCRYPT_OBJID_BLOB,
CERT_NAME_BLOB, *PCERT_NAME_BLOB,
CERT_RDN_VALUE_BLOB, *PCERT_RDN_VALUE_BLOB,
CERT_BLOB, *PCERT_BLOB,
CRL_BLOB, *PCRL_BLOB,
DATA_BLOB, *PDATA_BLOB,
CRYPT_DATA_BLOB, *PCRYPT_DATA_BLOB,
CRYPT_HASH_BLOB, *PCRYPT_HASH_BLOB,
CRYPT_DIGEST_BLOB, *PCRYPT_DIGEST_BLOB,
CRYPT_DER_BLOB, *PCRYPT_DER_BLOB,
CRYPT_ATTR_BLOB, *PCRYPT_ATTR_BLOB;
typedef struct _CRYPT_KEY_PROV_PARAM {
    DWORD           dwParam;
    BYTE            *pbData;
    DWORD           cbData;
    DWORD           dwFlags;
} CRYPT_KEY_PROV_PARAM, *PCRYPT_KEY_PROV_PARAM;
typedef struct _CRYPT_KEY_PROV_INFO {
    LPWSTR                  pwszContainerName;
    LPWSTR                  pwszProvName;
    DWORD                   dwProvType;
    DWORD                   dwFlags;
    DWORD                   cProvParam;
    PCRYPT_KEY_PROV_PARAM   rgProvParam;
    DWORD                   dwKeySpec;
} CRYPT_KEY_PROV_INFO, *PCRYPT_KEY_PROV_INFO;

#define X509_ASN_ENCODING  1


#define PROV_RSA_FULL        1
#define MS_DEF_PROV          L"Microsoft Base Cryptographic Provider v1.0"
#define CRYPT_SILENT         0x40
#define CRYPT_VERIFYCONTEXT  0xF0000000

#define CERT_KEY_PROV_HANDLE_PROP_ID        1
#define CERT_KEY_PROV_INFO_PROP_ID          2
#define CERT_SHA1_HASH_PROP_ID              3
#define CERT_MD5_HASH_PROP_ID               4
#define CERT_HASH_PROP_ID                   CERT_SHA1_HASH_PROP_ID
#define CERT_KEY_CONTEXT_PROP_ID            5
#define CERT_KEY_SPEC_PROP_ID               6
#define CERT_IE30_RESERVED_PROP_ID          7
#define CERT_PUBKEY_HASH_RESERVED_PROP_ID   8
#define CERT_ENHKEY_USAGE_PROP_ID           9
#define CERT_CTL_USAGE_PROP_ID              CERT_ENHKEY_USAGE_PROP_ID
#define CERT_NEXT_UPDATE_LOCATION_PROP_ID   10
#define CERT_FRIENDLY_NAME_PROP_ID          11
#define CERT_PVK_FILE_PROP_ID               12
#define CERT_DESCRIPTION_PROP_ID            13
#define CERT_ACCESS_STATE_PROP_ID           14
#define CERT_SIGNATURE_HASH_PROP_ID         15
#define CERT_SMART_CARD_DATA_PROP_ID        16
#define CERT_EFS_PROP_ID                    17
#define CERT_FORTEZZA_DATA_PROP_ID          18
#define CERT_ARCHIVED_PROP_ID               19
#define CERT_KEY_IDENTIFIER_PROP_ID         20
#define CERT_AUTO_ENROLL_PROP_ID            21
#define CERT_PUBKEY_ALG_PARA_PROP_ID        22
#define CERT_CROSS_CERT_DIST_POINTS_PROP_ID 23
#define CERT_ISSUER_PUBLIC_KEY_MD5_HASH_PROP_ID     24
#define CERT_SUBJECT_PUBLIC_KEY_MD5_HASH_PROP_ID    25
#define CERT_ENROLLMENT_PROP_ID             26
#define CERT_DATE_STAMP_PROP_ID             27
#define CERT_ISSUER_SERIAL_NUMBER_MD5_HASH_PROP_ID  28
#define CERT_SUBJECT_NAME_MD5_HASH_PROP_ID  29
#define CERT_EXTENDED_ERROR_INFO_PROP_ID    30

#define CERT_COMPARE_MASK           0xFFFF
#define CERT_COMPARE_SHIFT          16
#define CERT_COMPARE_ANY            0
#define CERT_COMPARE_SHA1_HASH      1
#define CERT_COMPARE_NAME           2
#define CERT_COMPARE_ATTR           3
#define CERT_COMPARE_MD5_HASH       4
#define CERT_COMPARE_PROPERTY       5
#define CERT_COMPARE_PUBLIC_KEY     6
#define CERT_COMPARE_HASH           CERT_COMPARE_SHA1_HASH
#define CERT_COMPARE_NAME_STR_A     7
#define CERT_COMPARE_NAME_STR_W     8
#define CERT_COMPARE_KEY_SPEC       9
#define CERT_COMPARE_ENHKEY_USAGE   10
#define CERT_COMPARE_CTL_USAGE      CERT_COMPARE_ENHKEY_USAGE
#define CERT_COMPARE_SUBJECT_CERT   11
#define CERT_COMPARE_ISSUER_OF      12
#define CERT_COMPARE_EXISTING       13
#define CERT_COMPARE_SIGNATURE_HASH 14
#define CERT_COMPARE_KEY_IDENTIFIER 15
#define CERT_COMPARE_CERT_ID        16
#define CERT_COMPARE_CROSS_CERT_DIST_POINTS 17

#define CERT_FIND_ANY           (CERT_COMPARE_ANY << CERT_COMPARE_SHIFT)
#define CERT_FIND_SHA1_HASH     (CERT_COMPARE_SHA1_HASH << CERT_COMPARE_SHIFT)
#define CERT_FIND_MD5_HASH      (CERT_COMPARE_MD5_HASH << CERT_COMPARE_SHIFT)
#define CERT_FIND_SIGNATURE_HASH (CERT_COMPARE_SIGNATURE_HASH << CERT_COMPARE_SHIFT)
#define CERT_FIND_KEY_IDENTIFIER (CERT_COMPARE_KEY_IDENTIFIER << CERT_COMPARE_SHIFT)
#define CERT_FIND_HASH          CERT_FIND_SHA1_HASH

typedef ICryptographicBufferStatics *HCRYPTPROV;

__forceinline BOOL CryptAcquireContext(HCRYPTPROV *phProv, LPCTSTR pszContainer, LPCTSTR pszProvider, DWORD dwProvType, DWORD dwFlags)
{
    static const WCHAR *className = L"Windows.Security.Cryptography.CryptographicBuffer";
    const UINT32 clen = wcslen(className);

    HSTRING hClassName = NULL;
    HSTRING_HEADER header;
    HRESULT hr = WindowsCreateStringReference(className, clen, &header, &hClassName);
    if (FAILED(hr)) {
        WindowsDeleteString(hClassName);
        return FALSE;
    }

    ICryptographicBufferStatics *cryptoStatics = NULL;
    hr = RoGetActivationFactory(hClassName, &IID_ICryptographicBufferStatics, (void**)&cryptoStatics);
    WindowsDeleteString(hClassName);

    if (FAILED(hr))
        return FALSE;

    *phProv = cryptoStatics;

    return TRUE;
}

__forceinline BOOL CryptReleaseContext(HCRYPTPROV phProv, DWORD dwFlags)
{
    HRESULT hr = ICryptographicBufferStatics_Release(phProv);
    return SUCCEEDED(hr) && dwFlags==0;
}

__forceinline BOOL CryptGenRandom(HCRYPTPROV phProv, DWORD dwLen, BYTE *pbBuffer)
{
    IBuffer *buffer = NULL;
    HRESULT hr = ICryptographicBufferStatics_GenerateRandom(phProv, dwLen, &buffer);
    if (FAILED(hr)) {
        return FALSE;
    }

    UINT32 olength;
    unsigned char *rnd = NULL;
    hr = ICryptographicBufferStatics_CopyToByteArray(phProv, buffer, &olength, (BYTE**)&rnd);
    if (FAILED(hr)) {
        IBuffer_Release(buffer);
        return FALSE;
    }
    memcpy(pbBuffer, rnd, dwLen);

    IBuffer_Release(buffer);
    return TRUE;
}

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0A00
__forceinline HCERTSTORE CertOpenSystemStore(HCRYPTPROV_LEGACY hprov, LPCSTR szSubsystemProtocol)
{
    static const WCHAR *className = L"Windows.Security.Cryptography.Certificates";
    const UINT32 clen = wcslen(className);

    HSTRING hClassName = NULL;
    HSTRING_HEADER header;
    HRESULT hr = WindowsCreateStringReference(className, clen, &header, &hClassName);
    if (FAILED(hr)) {
        WindowsDeleteString(hClassName);
        return NULL;
    }

    ICertificateStoresStatics *certStoresStatics = NULL;
    hr = RoGetActivationFactory(hClassName, &IID_ICertificateStoresStatics, (void**)&certStoresStatics);
    WindowsDeleteString(hClassName);

    if (FAILED(hr))
        return NULL;
    
    if (!strcmp(szSubsystemProtocol, "ROOT"))
    {
        ICertificateStore *result;
        hr = ICertificateStoresStatics_get_TrustedRootCertificationAuthorities(certStoresStatics, &result);
        ICertificateStoresStatics_Release(certStoresStatics);
        if (SUCCEEDED(hr))
        {
            return result;
        }
    }
    else if (!strcmp(szSubsystemProtocol, "CA"))
    {
        ICertificateStore *result;
        hr = ICertificateStoresStatics_get_IntermediateCertificationAuthorities(certStoresStatics, &result);
        ICertificateStoresStatics_Release(certStoresStatics);
        if (SUCCEEDED(hr))
        {
            return result;
        }
    }
    else
    {
        ICertificateStoresStatics_Release(certStoresStatics);
    }

    return NULL;
}

__forceinline BOOL WINAPI CertCloseStore(HCERTSTORE hCertStore, DWORD dwFlags)
{
    HRESULT hr = ICertificateStoresStatics_Release(hCertStore);
    return SUCCEEDED(hr);
}

__forceinline PCCERT_CONTEXT CertEnumCertificatesInStore(HCERTSTORE hCertStore, PCCERT_CONTEXT pPrevCertContext)
{
    return NULL;
}

__forceinline PCCRL_CONTEXT CertEnumCRLsInStore(HCERTSTORE hCertStore, PCCRL_CONTEXT pPrevCrlContext)
{
    return NULL;
}

#define Loaded_CertEnumCRLsInStore CertEnumCRLsInStore

__forceinline BOOL CertGetCertificateContextProperty(PCCERT_CONTEXT pCertContext, DWORD dwPropId, void *pvData, DWORD *pcbData)
{
    return FALSE;
}

__forceinline BOOL CertDeleteCertificateFromStore(PCCERT_CONTEXT pCertContext)
{
    return FALSE;
}

__forceinline HCERTSTORE PFXImportCertStore(CRYPT_DATA_BLOB* pPFX, LPCWSTR szPassword, DWORD dwFlags)
{
    return NULL;
}
#else
WINCRYPT32API HCERTSTORE WINAPI CertOpenStore(LPCSTR lpszStoreProvider, DWORD dwEncodingType, HCRYPTPROV_LEGACY hCryptProv, DWORD dwFlags, const void *pvPara);
WINCRYPT32API HCERTSTORE WINAPI CertOpenSystemStoreW(HCRYPTPROV_LEGACY hProv, LPCWSTR szSubsystemProtocol);
#define CertOpenSystemStore  CertOpenSystemStoreW
WINCRYPT32API BOOL WINAPI CertCloseStore(HCERTSTORE hCertStore, DWORD dwFlags);
WINCRYPT32API PCCRL_CONTEXT WINAPI CertEnumCRLsInStore(HCERTSTORE hCertStore, PCCRL_CONTEXT pPrevCrlContext);
WINCRYPT32API PCCERT_CONTEXT WINAPI CertEnumCertificatesInStore(HCERTSTORE hCertStore, PCCERT_CONTEXT pPrevCertContext);
WINCRYPT32API PCCERT_CONTEXT WINAPI CertFindCertificateInStore(HCERTSTORE hCertStore, DWORD dwCertEncodingType, DWORD dwFindFlags, DWORD dwFindType, const void *pvFindPara, PCCERT_CONTEXT pPrevCertContext);
WINCRYPT32API BOOL WINAPI CertGetCertificateContextProperty(PCCERT_CONTEXT pCertContext, DWORD dwPropId, void *pvData, DWORD *pcbData);
WINCRYPT32API BOOL WINAPI CertDeleteCertificateFromStore(PCCERT_CONTEXT pCertContext);
WINCRYPT32API HCERTSTORE WINAPI PFXImportCertStore(CRYPT_DATA_BLOB* pPFX, LPCWSTR szPassword, DWORD dwFlags);
#endif /* _WIN32_WINNT */

// By default, when the CurrentUser "Root" store is opened, any SystemRegistry
// roots not also on the protected root list are deleted from the cache before
// CertOpenStore() returns. Set the following flag to return all the roots
// in the SystemRegistry without checking the protected root list.
#define CERT_SYSTEM_STORE_UNPROTECTED_FLAG      0x40000000

#define CERT_SYSTEM_STORE_DEFER_READ_FLAG       0x20000000

// Location of the system store:
#define CERT_SYSTEM_STORE_LOCATION_MASK         0x00FF0000
#define CERT_SYSTEM_STORE_LOCATION_SHIFT        16


//  Registry: HKEY_CURRENT_USER or HKEY_LOCAL_MACHINE
#define CERT_SYSTEM_STORE_CURRENT_USER_ID       1
#define CERT_SYSTEM_STORE_LOCAL_MACHINE_ID      2
//  Registry: HKEY_LOCAL_MACHINE\Software\Microsoft\Cryptography\Services
#define CERT_SYSTEM_STORE_CURRENT_SERVICE_ID    4
#define CERT_SYSTEM_STORE_SERVICES_ID           5
//  Registry: HKEY_USERS
#define CERT_SYSTEM_STORE_USERS_ID              6

//  Registry: HKEY_CURRENT_USER\Software\Policies\Microsoft\SystemCertificates
#define CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY_ID    7
//  Registry: HKEY_LOCAL_MACHINE\Software\Policies\Microsoft\SystemCertificates
#define CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY_ID   8

//  Registry: HKEY_LOCAL_MACHINE\Software\Microsoft\EnterpriseCertificates
#define CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE_ID     9

#define CERT_SYSTEM_STORE_CURRENT_USER          \
    (CERT_SYSTEM_STORE_CURRENT_USER_ID << CERT_SYSTEM_STORE_LOCATION_SHIFT)
#define CERT_SYSTEM_STORE_LOCAL_MACHINE         \
    (CERT_SYSTEM_STORE_LOCAL_MACHINE_ID << CERT_SYSTEM_STORE_LOCATION_SHIFT)
#define CERT_SYSTEM_STORE_CURRENT_SERVICE       \
    (CERT_SYSTEM_STORE_CURRENT_SERVICE_ID << CERT_SYSTEM_STORE_LOCATION_SHIFT)
#define CERT_SYSTEM_STORE_SERVICES              \
    (CERT_SYSTEM_STORE_SERVICES_ID << CERT_SYSTEM_STORE_LOCATION_SHIFT)
#define CERT_SYSTEM_STORE_USERS                 \
    (CERT_SYSTEM_STORE_USERS_ID << CERT_SYSTEM_STORE_LOCATION_SHIFT)

#define CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY   \
    (CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY_ID << \
        CERT_SYSTEM_STORE_LOCATION_SHIFT)
#define CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY  \
    (CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY_ID << \
        CERT_SYSTEM_STORE_LOCATION_SHIFT)

#define CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE  \
    (CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE_ID << \
        CERT_SYSTEM_STORE_LOCATION_SHIFT)

//+-------------------------------------------------------------------------
//  Certificate Store Provider Types
//--------------------------------------------------------------------------
#define CERT_STORE_PROV_MSG                 ((LPCSTR) 1)
#define CERT_STORE_PROV_MEMORY              ((LPCSTR) 2)
#define CERT_STORE_PROV_FILE                ((LPCSTR) 3)
#define CERT_STORE_PROV_REG                 ((LPCSTR) 4)

#define CERT_STORE_PROV_PKCS7               ((LPCSTR) 5)
#define CERT_STORE_PROV_SERIALIZED          ((LPCSTR) 6)
#define CERT_STORE_PROV_FILENAME_A          ((LPCSTR) 7)
#define CERT_STORE_PROV_FILENAME_W          ((LPCSTR) 8)
#define CERT_STORE_PROV_FILENAME            CERT_STORE_PROV_FILENAME_W
#define CERT_STORE_PROV_SYSTEM_A            ((LPCSTR) 9)
#define CERT_STORE_PROV_SYSTEM_W            ((LPCSTR) 10)
#define CERT_STORE_PROV_SYSTEM              CERT_STORE_PROV_SYSTEM_W

#define CERT_STORE_PROV_COLLECTION          ((LPCSTR) 11)
#define CERT_STORE_PROV_SYSTEM_REGISTRY_A   ((LPCSTR) 12)
#define CERT_STORE_PROV_SYSTEM_REGISTRY_W   ((LPCSTR) 13)
#define CERT_STORE_PROV_SYSTEM_REGISTRY     CERT_STORE_PROV_SYSTEM_REGISTRY_W
#define CERT_STORE_PROV_PHYSICAL_W          ((LPCSTR) 14)
#define CERT_STORE_PROV_PHYSICAL            CERT_STORE_PROV_PHYSICAL_W

// SmartCard Store Provider isn't supported
#define CERT_STORE_PROV_SMART_CARD_W        ((LPCSTR) 15)
#define CERT_STORE_PROV_SMART_CARD          CERT_STORE_PROV_SMART_CARD_W

#define CERT_STORE_PROV_LDAP_W              ((LPCSTR) 16)
#define CERT_STORE_PROV_LDAP                CERT_STORE_PROV_LDAP_W
#define CERT_STORE_PROV_PKCS12              ((LPCSTR) 17)

#define sz_CERT_STORE_PROV_MEMORY           "Memory"
#define sz_CERT_STORE_PROV_FILENAME_W       "File"
#define sz_CERT_STORE_PROV_FILENAME         sz_CERT_STORE_PROV_FILENAME_W
#define sz_CERT_STORE_PROV_SYSTEM_W         "System"
#define sz_CERT_STORE_PROV_SYSTEM           sz_CERT_STORE_PROV_SYSTEM_W
#define sz_CERT_STORE_PROV_PKCS7            "PKCS7"
#define sz_CERT_STORE_PROV_PKCS12           "PKCS12"
#define sz_CERT_STORE_PROV_SERIALIZED       "Serialized"

#define sz_CERT_STORE_PROV_COLLECTION       "Collection"
#define sz_CERT_STORE_PROV_SYSTEM_REGISTRY_W "SystemRegistry"
#define sz_CERT_STORE_PROV_SYSTEM_REGISTRY  sz_CERT_STORE_PROV_SYSTEM_REGISTRY_W
#define sz_CERT_STORE_PROV_PHYSICAL_W       "Physical"
#define sz_CERT_STORE_PROV_PHYSICAL         sz_CERT_STORE_PROV_PHYSICAL_W

// SmartCard Store Provider isn't supported
#define sz_CERT_STORE_PROV_SMART_CARD_W     "SmartCard"
#define sz_CERT_STORE_PROV_SMART_CARD       sz_CERT_STORE_PROV_SMART_CARD_W

#define sz_CERT_STORE_PROV_LDAP_W           "Ldap"
#define sz_CERT_STORE_PROV_LDAP             sz_CERT_STORE_PROV_LDAP_W


typedef struct  _CRYPTPROTECT_PROMPTSTRUCT
{
    DWORD cbSize;
    DWORD dwPromptFlags;
    HWND  hwndApp;
    LPCWSTR szPrompt;
} CRYPTPROTECT_PROMPTSTRUCT, *PCRYPTPROTECT_PROMPTSTRUCT;

#define CRYPTPROTECT_UI_FORBIDDEN        0x1

BOOL
WINAPI
CryptProtectData(
    DATA_BLOB*      pDataIn,
    LPCWSTR         szDataDescr,
    DATA_BLOB*      pOptionalEntropy,
    PVOID           pvReserved,
    CRYPTPROTECT_PROMPTSTRUCT*  pPromptStruct,
    DWORD           dwFlags,
    DATA_BLOB*      pDataOut            // out encr blob
    );

BOOL
WINAPI
CryptUnprotectData(
    DATA_BLOB*      pDataIn,             // in encr blob
    LPWSTR*     ppszDataDescr,       // out
    DATA_BLOB*      pOptionalEntropy,
    PVOID           pvReserved,
    CRYPTPROTECT_PROMPTSTRUCT*  pPromptStruct,
    DWORD           dwFlags,
    DATA_BLOB*      pDataOut
    );

#endif /* WINAPI_FAMILY */

#endif /* no wincrypt */

#endif /* _WINSTORE_WINCRYPT_H */
