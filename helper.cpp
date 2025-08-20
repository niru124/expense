#include "helper.h"
#include <ctime>
#include <iomanip>
#include <sstream>

// Function to format date from DD-MM-YYYY to YYYY-MM-DD
std::string format_date(const std::string& date_str) {
    std::tm t = {};
    std::istringstream ss(date_str);
    ss >> std::get_time(&t, "%d-%m-%Y");
    if (ss.fail()) {
        // Handle parsing error, perhaps return an empty string or throw an exception
        return ""; 
    }
    std::ostringstream oss;
    oss << std::put_time(&t, "%Y-%m-%d");
    return oss.str();
}