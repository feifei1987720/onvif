#ifndef ONVIF_AUTH
#define ONVIF_AUTH


#ifdef __cplusplus
extern "C" {
#endif


const char *wsse_PasswordTextURI = "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordText";
const char *wsse_PasswordDigestURI = "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest";





/** SHA1 hash */
#define SOAP_SMD_SHA1	(0x01)
/** SHA256 hash */
#define SOAP_SMD_SHA256	(0x02)
/** SHA512 hash */
#define SOAP_SMD_SHA512	(0x03)	

/** Digest */
#define SOAP_SMD_DGST	(0x04)
/** Sign */
#define SOAP_SMD_SIGN	(0x08)
/** Verify */
#define SOAP_SMD_VRFY	(0x0C)

#define SOAP_SMD_MD5	(0x00)

#define SOAP_SMD_HASH	(0x0003)

/** Size of the random nonce */
#define SOAP_WSSE_NONCELEN	(20)
/** SHA1 digest size in octets */
#define SOAP_SMD_SHA1_SIZE	(20)

#define SOAP_SMD_ALGO	(0x000C)

#define SOAP_SMD_HMAC	(0x00)

/** DGST-SHA1 digest algorithm */
#define SOAP_SMD_DGST_SHA1	 (SOAP_SMD_DGST | SOAP_SMD_SHA1)							

struct soap_smd_data
{ int alg;		/**< The digest or signature algorithm used */
  void *ctx;		/**< EVP_MD_CTX or HMAC_CTX */
  const void *key;	/**< EVP_PKEY */
  int (*fsend)(struct soap*, const char*, size_t);
  size_t (*frecv)(struct soap*, char*, size_t);
  soap_mode mode;	/**< to preserve soap->mode value */
};









int soap_wsse_add_UsernameTokenDigest(struct soap *soap, const char *id, const char *username, const char *password);



#ifdef __cplusplus
}
#endif

#endif
