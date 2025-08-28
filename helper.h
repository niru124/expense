#ifndef HELPER_H
#define HELPER_H

#include <string>

// Function to format date from DD-MM-YYYY to YYYY-MM-DD
std::string format_date(const std::string &date_str); // Declaration only
std::string refinedString(const std::string &str);
template <typename T> bool isNumber(const T &a) {
  return std::is_arithmetic<T>::value;
}

#endif // HELPER_H
