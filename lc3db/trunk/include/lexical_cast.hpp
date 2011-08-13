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

template <typename Target, typename Source> 
Target lexical_cast(const Source &arg);

template<>
uint16_t lexical_cast<uint16_t, std::string>(const std::string &str)
{
  std::string input = ((str[0] | 0x20) == 'x') ? "0" + str : str;
  char *end = 0;
  uint32_t value = strtoul(input.c_str(), &end, 0);

  if (!end || *end || value >= 0x10000 || end == input.c_str()) {
    throw bad_lexical_cast();
  }

  return value & 0xFFFF;
}

template<>
int16_t lexical_cast<int16_t, std::string>(const std::string &str)
{
  std::string input = ((str[0] | 0x20) == 'x') ? "0" + str : str;
  char *end = 0;
  int32_t value = strtoul(input.c_str(), &end, 0);

  if (!end || *end || value >= 0x10000 || value < (-0x8000) || end == input.c_str()) {
    throw bad_lexical_cast();
  }

  return value & 0xFFFF;
}

#endif
