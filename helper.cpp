#include "helper.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdbool.h>

// Function to format date from DD-MM-YYYY to YYYY-MM-DD
std::string format_date(const std::string &date_str) {
  std::tm t = {};
  std::istringstream ss(date_str);
  ss >> std::get_time(&t, "%d-%m-%Y");
  if (ss.fail()) {
    // Handle parsing error, perhaps return an empty string or throw an
    // exception
    return "";
  }
  std::ostringstream oss;
  oss << std::put_time(&t, "%Y-%m-%d");
  return oss.str();
}

std::string refinedString(const std::string &str) {
  bool isSpace = false;
  std::string res;
  int i = 0;
  while (str[i] == ' ') {
    i++;
  }

  while (i != str.size()) {
    if (str[i] == ' ' && !isSpace) {
      isSpace = true;
    } else if (str[i] == ' ' && isSpace) {
      i++;
      continue;
    } else if (isalnum(str[i]) && isSpace) {
      res += toupper(str[i]);
      isSpace = false;
      i++;
      continue;
    } else if (isalnum(str[i])) {
      isSpace = false;
    }
    res += str[i];
    i++;
  }
  res[0] = toupper(res[0]);
  return res;
}

template <typename T> bool isNumber(const T &a) {
  return std::is_arithmetic<T>::value;
}
