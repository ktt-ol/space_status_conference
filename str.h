#pragma once

#include <string.h>

class Str {
public:
  inline Str(const char* const string, const size_t length)
    : ptr(string)
    , len(length)
  { }
  inline bool operator== (const char* str) const {
    const size_t rlen = strlen(str);
    if (rlen != this->len)
      return false;
    return !strncmp(ptr, str, len);
  }
private:
  const char* const ptr;
  const size_t len;
};