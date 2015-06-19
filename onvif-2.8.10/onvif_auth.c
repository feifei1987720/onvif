#include <stdio.h>
#include <stdlib.h>

#include "soapH.h"
#include "soapStub.h"

#include "openssl/evp.h"
#include "openssl/hmac.h"
#include "onvif_auth.h"


static int soap_smd_check(struct soap *soap, struct soap_smd_data *data, int err, const char *msg)
{ 
	if (err <= 0)
  {
    unsigned long r;
    while ((r = ERR_get_error()))
    { ERR_error_string_n(r, soap->msgbuf, sizeof(soap->msgbuf));
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "-- SMD Error (%d) %s: %s\n", err, msg, soap->msgbuf));
    }
    if (data->ctx)
    { if ((data->alg & SOAP_SMD_ALGO) == SOAP_SMD_HMAC)
        HMAC_CTX_cleanup((HMAC_CTX*)data->ctx); 
      else
        EVP_MD_CTX_cleanup((EVP_MD_CTX*)data->ctx);
      SOAP_FREE(soap, data->ctx);
      data->ctx = NULL;
    }
    return soap_set_receiver_error(soap, msg, soap->msgbuf, SOAP_SSL_ERROR);
  }
  return SOAP_OK;
}


int soap_smd_init(struct soap *soap, struct soap_smd_data *data, int alg, const void *key, int keylen)
{ 

  int err = 1;
  const EVP_MD *type;
  soap_ssl_init();
  /* the algorithm to use */
  data->alg = alg;
  /* the key to use */
  data->key = key;
  /* allocate and init the OpenSSL HMAC or EVP_MD context */
  if ((alg & SOAP_SMD_ALGO) == SOAP_SMD_HMAC)
  { data->ctx = (void*)SOAP_MALLOC(soap, sizeof(HMAC_CTX));
    HMAC_CTX_init((HMAC_CTX*)data->ctx);
  }
  else
  { 
	  /*******************************************************************************************/
	 //此处分配内存有问题，在soap_smd_final中向此处拷贝了20字节的数据，但此处却只分配了16字节的内存，所以将内存大小乘以2
	 /********************************************************************************************/
	data->ctx = (void*)SOAP_MALLOC(soap, sizeof(EVP_MD_CTX)*2);  
    EVP_MD_CTX_init((EVP_MD_CTX*)data->ctx);
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "-- SMD Init alg=%x (%p) --\n", alg, data->ctx));
  /* init the digest or signature computations */
  switch (alg & SOAP_SMD_HASH)
  { case SOAP_SMD_MD5:
      type = EVP_md5();
      break;
    case SOAP_SMD_SHA1:
      type = EVP_sha1();
      break;
    case SOAP_SMD_SHA256:
      type = EVP_sha256();
      break;
    case SOAP_SMD_SHA512:
      type = EVP_sha512();
      break;
    default:
      type = EVP_md_null();
  }
  switch (alg & SOAP_SMD_ALGO)
  { case SOAP_SMD_HMAC:
      HMAC_Init((HMAC_CTX*)data->ctx, key, keylen, type);
      break;
    case SOAP_SMD_DGST:
      EVP_DigestInit((EVP_MD_CTX*)data->ctx, type);
      break;
    case SOAP_SMD_SIGN:
      err = EVP_SignInit((EVP_MD_CTX*)data->ctx, type);
      break;
    case SOAP_SMD_VRFY:
      err = EVP_VerifyInit((EVP_MD_CTX*)data->ctx, type);
      break;
    default:
      break;
  }
  /* check and return */

  return soap_smd_check(soap, data, err, "soap_smd_init() failed");
}

int soap_smd_update(struct soap *soap, struct soap_smd_data *data, const char *buf, size_t len)
{ 
  int err = 1;
  if (!data->ctx)
    return soap_set_receiver_error(soap, "soap_smd_update() failed", "No context", SOAP_SSL_ERROR);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "-- SMD Update alg=%x n=%lu (%p) --\n", data->alg, (unsigned long)len, data->ctx));
  switch (data->alg & SOAP_SMD_ALGO)
  { case SOAP_SMD_HMAC:
      HMAC_Update((HMAC_CTX*)data->ctx, (const unsigned char*)buf, len);
      break;
    case SOAP_SMD_DGST:
      EVP_DigestUpdate((EVP_MD_CTX*)data->ctx, (const void*)buf, (unsigned int)len);
      break;
    case SOAP_SMD_SIGN:
      err = EVP_SignUpdate((EVP_MD_CTX*)data->ctx, (const void*)buf, (unsigned int)len);
      break;
    case SOAP_SMD_VRFY:
      err = EVP_VerifyUpdate((EVP_MD_CTX*)data->ctx, (const void*)buf, (unsigned int)len);
      break;
  }
  DBGMSG(TEST, buf, len);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "\n--"));
  /* check and return */
  return soap_smd_check(soap, data, err, "soap_smd_update() failed");
}


int soap_smd_final(struct soap *soap, struct soap_smd_data *data, char *buf, int *len)
{ 
	unsigned int n = 0;
  int err = 1;
  if (!data->ctx)
    return soap_set_receiver_error(soap, "soap_smd_final() failed", "No context", SOAP_SSL_ERROR);
  if (buf)
  { /* finalize the digest or signature computation */
   
    switch (data->alg & SOAP_SMD_ALGO)
    { case SOAP_SMD_HMAC:
        HMAC_Final((HMAC_CTX*)data->ctx, (unsigned char*)buf, &n);
        HMAC_CTX_cleanup((HMAC_CTX*)data->ctx);
        break;
      case SOAP_SMD_DGST:
        EVP_DigestFinal((EVP_MD_CTX*)data->ctx, (unsigned char*)buf, &n);
        break;
      case SOAP_SMD_SIGN:
        err = EVP_SignFinal((EVP_MD_CTX*)data->ctx, (unsigned char*)buf, &n, (EVP_PKEY*)data->key);
        break;
      case SOAP_SMD_VRFY:
        if (len)
        { n = (unsigned int)*len;
          err = EVP_VerifyFinal((EVP_MD_CTX*)data->ctx, (unsigned char*)buf, n, (EVP_PKEY*)data->key);
        
        }
        else
          err = 0;
        break;
    }
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "-- SMD Final alg=%x (%p) %d bytes--\n", data->alg, data->ctx, n));
    DBGHEX(TEST, buf, n);
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "\n--"));
    /* return length of digest or signature produced */
    if (len)
      *len = (int)n;
  }
   
   SOAP_FREE(soap, data->ctx);
   
  data->ctx = NULL;
  /* check and return */
  return soap_smd_check(soap, data, err, "soap_smd_final() failed");
}




struct _wsse__Security* soap_wsse_add_Security(struct soap *soap)
{ 
	DBGFUN("soap_wsse_add_Security");
  /* if we don't have a SOAP Header, create one */
  soap_header(soap);
  /* if we don't have a wsse:Security element in the SOAP Header, create one */
  if (!soap->header->wsse__Security)
  { soap->header->wsse__Security = (_wsse__Security*)soap_malloc(soap, sizeof(_wsse__Security));
    if (!soap->header->wsse__Security)
      return NULL;
    soap_default__wsse__Security(soap, soap->header->wsse__Security);
  }
  return soap->header->wsse__Security;
}


static void
calc_digest(struct soap *soap, const char *created, const char *nonce, int noncelen, const char *password, char hash[SOAP_SMD_SHA1_SIZE])
{
    struct soap_smd_data context;
  /* use smdevp engine */
  soap_smd_init(soap, &context, SOAP_SMD_DGST_SHA1, NULL, 0);
  
  soap_smd_update(soap, &context, nonce, noncelen);

  soap_smd_update(soap, &context, created, strlen(created));

  soap_smd_update(soap, &context, password, strlen(password));

  soap_smd_final(soap, &context, hash, NULL);

}


static void calc_nonce(struct soap *soap, char nonce[SOAP_WSSE_NONCELEN])
{ 
  int i;
  time_t r = time(NULL);
  memcpy(nonce, &r, 4);
  for (i = 4; i < SOAP_WSSE_NONCELEN; i += 4)
  { r = soap_random;
    memcpy(nonce + i, &r, 4);
  }
}


int soap_wsse_add_UsernameTokenText(struct soap *soap, const char *id, const char *username, const char *password)
{ 
	
 _wsse__Security *security = soap_wsse_add_Security(soap);
 
   DBGFUN2("soap_wsse_add_UsernameTokenText", "id=%s", id?id:"", "username=%s", username?username:"");
  /* allocate a UsernameToken if we don't have one already */
  if (!security->UsernameToken)
  { if (!(security->UsernameToken = (_wsse__UsernameToken*)soap_malloc(soap, sizeof(_wsse__UsernameToken))))
      return soap->error = SOAP_EOM;
  }
  soap_default__wsse__UsernameToken(soap, security->UsernameToken);
  /* populate the UsernameToken */
  security->UsernameToken->wsu__Id = soap_strdup(soap, id);
  security->UsernameToken->Username = soap_strdup(soap, username);
  /* allocate and populate the Password */
  if (password)
  { if (!(security->UsernameToken->Password = (_wsse__Password*)soap_malloc(soap, sizeof(_wsse__Password))))
      return soap->error = SOAP_EOM;
    soap_default__wsse__Password(soap, security->UsernameToken->Password);
    security->UsernameToken->Password->Type = (char*)wsse_PasswordTextURI;
    security->UsernameToken->Password->__item = soap_strdup(soap, password);
  }
  return SOAP_OK;
}


int soap_wsse_add_UsernameTokenDigest(struct soap *soap, const char *id, const char *username, const char *password)
{ 

   _wsse__Security *security = soap_wsse_add_Security(soap);
  time_t now = time(NULL);

  const char *created = soap_dateTime2s(soap, now);
  char HA[SOAP_SMD_SHA1_SIZE], HABase64[29];
  char nonce[SOAP_WSSE_NONCELEN], *nonceBase64;
  DBGFUN2("soap_wsse_add_UsernameTokenDigest", "id=%s", id?id:"", "username=%s", username?username:"");
  /* generate a nonce */
  calc_nonce(soap, nonce);
 
  nonceBase64 = soap_s2base64(soap, (unsigned char*)nonce, NULL, SOAP_WSSE_NONCELEN);
   
  /* The specs are not clear: compute digest over binary nonce or base64 nonce? */
  /* compute SHA1(created, nonce, password) */
  calc_digest(soap, created, nonce, SOAP_WSSE_NONCELEN, password, HA);
  /* Hm...?
  calc_digest(soap, created, nonceBase64, strlen(nonceBase64), password, HA);
  */
  
  soap_s2base64(soap, (unsigned char*)HA, HABase64, SOAP_SMD_SHA1_SIZE);
  /* populate the UsernameToken with digest */
  soap_wsse_add_UsernameTokenText(soap, id, username, HABase64);
 
  /* populate the remainder of the password, nonce, and created */
  security->UsernameToken->Password->Type = (char*)wsse_PasswordDigestURI;
  security->UsernameToken->Nonce = nonceBase64;
  security->UsernameToken->wsu__Created = soap_strdup(soap, created);
  return SOAP_OK;
}
