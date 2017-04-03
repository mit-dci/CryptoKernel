#ifndef BASE64_H_INCLUDED
#define BASE64_H_INCLUDED

#include <string>

std::string base64_encode(unsigned char const* btyes_to_encode, unsigned int len);
std::string base64_decode(std::string const& s);

#endif // BASE64_H_INCLUDED

