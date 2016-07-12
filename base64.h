#ifndef _BASE_64_H_
#define _BASE_64_H_

//int base64_decode(size_t in_len, const char *in, size_t out_len, unsigned char *out);
int base64_encode(size_t in_len, const unsigned char *in, size_t out_len, char *out);

#endif // _BASE_64_H_
