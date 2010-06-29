#ifndef MD5_H
#define MD5_H

#define MD5_DIGEST_LENGTH 16

/* Data structure for MD5 (Message Digest) computation */
typedef struct {
  u_int32_t count[2];                   /* number of _bits_ handled mod 2^64 */
  u_int32_t state[4];                                    /* scratch buffer */
  unsigned char buffer[64];                              /* input buffer */
  unsigned char digest[16];     /* actual digest after MD5Final call */
} MD5_CTX;

void
MD5Init(MD5_CTX *context);

void
MD5Update(MD5_CTX *context, const void *inpp, unsigned int inputLen);

void
MD5Final(unsigned char digest[MD5_DIGEST_LENGTH], MD5_CTX *context);

#endif /* MD5_H */
