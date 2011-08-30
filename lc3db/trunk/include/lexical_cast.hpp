#ifndef LEXICAL_CAST_HPP
#define LEXICAL_CAST_HPP

#include <exception>
#include <string>
#include <cstdlib>
#include <typeinfo>

class bad_lexical_cast : public std::bad_cast { 
  const char *what() const throw () {
    return "Bad Lexical Cast";
  }
};

template <typename Target> 
Target lexical_cast(const std::string &arg);
template <typename Target> 
Target lexical_cast(const char *arg);

template<>
uint16_t lexical_cast<uint16_t>(const char *str)
{
  char *end = 0;
  int base = 0;
  if ((str[0]|0x20)=='x') {	// handle the lc3 hex (x1234)
	  str++;
	  base = 16;
  }
  long value = strtol(str, &end, base);
  if (!end || *end || value >= 0x10000 || end == str) {
    throw bad_lexical_cast();
  }

  return value & 0xFFFF;
}

template<>
int16_t lexical_cast<int16_t>(const char * str)
{
  char *end = 0;
  int base = 0;
  if ((str[0]|0x20)=='x') {	// handle the lc3 hex (x1234)
	  str++;
	  base = 16;
  }
  long value = strtol(str, &end, base);
  if (!end || *end || value >= 0x10000 || value < (-0x8000) || end == str) {
    throw bad_lexical_cast();
  }

  return value & 0xFFFF;
}

template<>
uint16_t lexical_cast<uint16_t>(const std::string &str)
{
  return lexical_cast<uint16_t>(str.c_str());
}

template<>
int16_t lexical_cast<int16_t>(const std::string &str)
{
  return lexical_cast<int16_t>(str.c_str());
}

#endif
