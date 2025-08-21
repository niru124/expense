#include "helper.h"
#include "FinanceDB.h"
#include<vector>
#include<queue>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <numeric>
#include <ctime>

// --- All the code from the previous FinanceDB.cpp goes here ---
// --- (Constructors, destructors, addExpense, etc.)     ---
// --- I am omitting it for brevity, but you should keep it. ---
// --- Just add the new function implementations below.      ---

// Helper function to get current month and year string
std::string getCurrentYearMonth() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_local;
    localtime_r(&now_time, &tm_local);
    std::stringstream ss;
    ss << std::put_time(&tm_local, "%m_%Y");
    return ss.str();
}

// Helper function to get current day, month, and year string
std::string getCurrentDayMonthYear() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_local;
    localtime_r(&now_time, &tm_local);
    std::stringstream ss;
    ss << std::put_time(&tm_local, "%d_%m_%Y");
    return ss.str();
}

// constructor
FinanceDB::FinanceDB(const std::string& mainDbPath, const std::string& detailedDbPath)
    : mainDB(nullptr), detailedDB(nullptr) {
    
    currentYearMonth = getCurrentYearMonth();
    currentTableName = "expenses_" + currentYearMonth;

    if (sqlite3_open(mainDbPath.c_str(), &mainDB)) {
        std::cerr << "Error opening Main DB: " << sqlite3_errmsg(mainDB) << std::endl;
        mainDB = nullptr;
    } else {
        std::cout << "Main DB opened successfully." << std::endl;
        initMainDB();
    }

    if (sqlite3_open(detailedDbPath.c_str(), &detailedDB)) {
        std::cerr << "Error opening Detailed DB: " << sqlite3_errmsg(detailedDB) << std::endl;
        detailedDB = nullptr;
    } else {
        std::cout << "Detailed DB opened successfully." << std::endl;
        initDetailedDB();
    }
}

FinanceDB::~FinanceDB() {
    if (mainDB) sqlite3_close(mainDB);
    if (detailedDB) sqlite3_close(detailedDB);
    std::cout << "Database connections closed." << std::endl;
}

void FinanceDB::executeSQL(sqlite3* db, const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

void FinanceDB::initMainDB() {
    std::string sql = "CREATE TABLE IF NOT EXISTS Overall ("
                      "month_year TEXT PRIMARY KEY,"
                      "Salary REAL NOT NULL,"
                      "LimitAmount REAL NOT NULL,"
                      "SavingPercentage REAL,"
                      "Condition TEXT);";
    executeSQL(mainDB, sql);
}

void FinanceDB::initDetailedDB() {
    std::string sql = "CREATE TABLE IF NOT EXISTS " + currentTableName + " ("
                      "day_month_year TEXT PRIMARY KEY,"
                      "SpentOn TEXT NOT NULL,"
                      "Price REAL NOT NULL,"
                        "Priority INTEGER DEFAULT 0);";
    executeSQL(detailedDB, sql);
}

double FinanceDB::calculateCurrentSavings(double salary) {
    std::string sql = "SELECT Price FROM " + currentTableName + ";";
    sqlite3_stmt* stmt;
    double totalSpent = 0.0;

    if (sqlite3_prepare_v2(detailedDB, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            totalSpent += sqlite3_column_double(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);

    if (salary > 0) {
        double saved = salary - totalSpent;
        return (saved / salary) * 100.0;
    }
    return 0.0;
}

std::string FinanceDB::determineCondition(double savingPercentage) {
    if (savingPercentage >= 25.0) return "Good";
    if (savingPercentage >= 10.0) return "Needs Improvement";
    return "Bad";
}


bool FinanceDB::addOrUpdateMonthlySummary(double salary, double limit) {
    if (!mainDB) return false;

    double savingPercentage = calculateCurrentSavings(salary);
    std::string condition = determineCondition(savingPercentage);

    std::string sql = "INSERT INTO Overall (month_year, Salary, LimitAmount, SavingPercentage, Condition) VALUES (?, ?, ?, ?, ?)"
                      "ON CONFLICT(month_year) DO UPDATE SET "
                      "Salary=excluded.Salary, LimitAmount=excluded.LimitAmount, SavingPercentage=excluded.SavingPercentage, Condition=excluded.Condition;";
                      // This updates the existing row instead of inserting a new one. It uses the excluded keyword, which refers to the values that were attempted to be inserted.
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(mainDB, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(mainDB) << std::endl;
        return false;
    }

    // 1,2,3,4,5 denotes the placeholder of ?,?,?,?,?
    sqlite3_bind_text(stmt, 1, currentYearMonth.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, salary);
    sqlite3_bind_double(stmt, 3, limit);
    sqlite3_bind_double(stmt, 4, savingPercentage);
    sqlite3_bind_text(stmt, 5, condition.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Execution failed: " << sqlite3_errmsg(mainDB) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool FinanceDB::addExpense(const std::string& spentOn, double price) {
    if (!detailedDB) return false;

    std::string sql = "INSERT INTO " + currentTableName + " (day_month_year, SpentOn, Price, Priority) VALUES (?, ?, ?, 0);";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(detailedDB, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(detailedDB) << std::endl;
        return false;
    }

    std::string uniqueDayKey = getCurrentDayMonthYear() + "_" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());

    sqlite3_bind_text(stmt, 1, uniqueDayKey.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, spentOn.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, price);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Execution failed: " << sqlite3_errmsg(detailedDB) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    
    updatePriority(spentOn);

    return true;
}

void FinanceDB::updatePriority(const std::string& spentOn) {
    // Step 1: Get the count of the item
    std::string count_sql = "SELECT COUNT(*) FROM " + currentTableName + " WHERE SpentOn = ?;";
    sqlite3_stmt* count_stmt;
    int count = 0;
    if (sqlite3_prepare_v2(detailedDB, count_sql.c_str(), -1, &count_stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(count_stmt, 1, spentOn.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(count_stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(count_stmt, 0);
        }
    }
    sqlite3_finalize(count_stmt);

    // Step 2: Update the priority for all items with this name
    std::string update_sql = "UPDATE " + currentTableName + " SET Priority = ? WHERE SpentOn = ?;";
    sqlite3_stmt* update_stmt;
    if (sqlite3_prepare_v2(detailedDB, update_sql.c_str(), -1, &update_stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(update_stmt, 1, count);
        sqlite3_bind_text(update_stmt, 2, spentOn.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(update_stmt) != SQLITE_DONE) {
            std::cerr << "Execution failed: " << sqlite3_errmsg(detailedDB) << std::endl;
        }
    }
    sqlite3_finalize(update_stmt);
}


/***************************************************/
/********** NEW FUNCTIONS FOR VIEWING DATA *********/
/***************************************************/

std::vector<ExpenseRecord> FinanceDB::getRangeOfDate(std::string start_date,std::string end_date){
    std::vector<ExpenseRecord>summaries; 
    
    std::string sql="SELECT day_month_year, SpentOn, Price, Priority FROM "+currentTableName+" WHERE day_month_year BETWEEN ? AND ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(detailedDB, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, start_date.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, end_date.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ExpenseRecord e;
            e.day_month_year = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            e.spent_on = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            e.price = sqlite3_column_double(stmt, 2);
            e.priority = sqlite3_column_int(stmt, 3);
            summaries.push_back(e);
        }
    } else {
        std::cerr << "Failed to prepare statement for getRangeOfDate: " << sqlite3_errmsg(detailedDB) << std::endl;
    }
    sqlite3_finalize(stmt);
    return summaries;
}

std::vector<ExpenseRecord> FinanceDB::calcSortByPrice(bool order){
    std::vector<ExpenseRecord>summaries; 
    std::string ordering=(order)?"ASC":"DESC";
    std::string sql="SELECT day_month_year, SpentOn, Price, Priority FROM "+currentTableName+" ORDER BY Price "+ordering;
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(detailedDB, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ExpenseRecord e;
            e.day_month_year = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            e.spent_on = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            e.price = sqlite3_column_double(stmt, 2);
            e.priority = sqlite3_column_int(stmt, 3);
            summaries.push_back(e);
        }
    }
    sqlite3_finalize(stmt);
    return summaries;
}

std::vector<MonthlySummary> FinanceDB::getAllSummaries() {
    std::vector<MonthlySummary> summaries;
    std::string sql = "SELECT * FROM Overall;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(mainDB, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MonthlySummary s;
            s.month_year = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            s.salary = sqlite3_column_double(stmt, 1);
            s.limit = sqlite3_column_double(stmt, 2);
            s.saving_percentage = sqlite3_column_double(stmt, 3);
            s.condition = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            summaries.push_back(s);
        }
    }
    sqlite3_finalize(stmt);
    return summaries;
}

std::vector<ExpenseRecord> FinanceDB::getSortedByVal() {
    std::vector<ExpenseRecord> summaries;
    std::string sql = "SELECT day_month_year, SpentOn, Price, Priority FROM " + currentTableName + " ORDER BY Price DESC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(detailedDB, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ExpenseRecord e;
            e.day_month_year = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            e.spent_on = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            e.price = sqlite3_column_double(stmt, 2);
            e.priority = sqlite3_column_int(stmt, 3);
            summaries.push_back(e);
        }
    }
    sqlite3_finalize(stmt);
    return summaries;
}

std::vector<ExpenseRecord> FinanceDB::calcPriority() {
    // SQL to get SpentOn, average price, and count of occurrences (priority)
    // Ordered by count (priority) in descending order
    std::string sql = "SELECT SpentOn, AVG(Price), COUNT(*) AS num_occurrences FROM " + currentTableName + " GROUP BY SpentOn ORDER BY num_occurrences DESC;";
    sqlite3_stmt* stmt;
    std::vector<ExpenseRecord> ordered; // This will hold the aggregated and sorted data

    if (sqlite3_prepare_v2(detailedDB, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ExpenseRecord e;
            e.day_month_year = ""; // Not applicable for grouped data
            e.spent_on = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)); // SpentOn (column 0)
            e.price = sqlite3_column_double(stmt, 1); // AVG(Price) (column 1)
            e.priority = sqlite3_column_int(stmt, 2); // COUNT(*) as num_occurrences (column 2)
            ordered.push_back(e);
        }
    } else {
        std::cerr << "Failed to prepare statement for calcPriority: " << sqlite3_errmsg(detailedDB) << std::endl;
    }
    sqlite3_finalize(stmt);
    return ordered;
}

std::vector<ExpenseRecord> FinanceDB::getExpensesForMonth(const std::string& monthYear) {
    std::vector<ExpenseRecord> expenses;
    std::string tableName = "expenses_" + monthYear;
    std::string sql = "SELECT day_month_year, SpentOn, Price, Priority FROM " + tableName + ";";
    sqlite3_stmt* stmt;

    // Check if the table exists by preparing the statement. If it fails, return empty.
    if (sqlite3_prepare_v2(detailedDB, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ExpenseRecord e;
            e.day_month_year = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            e.spent_on = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            e.price = sqlite3_column_double(stmt, 2);
            e.priority = sqlite3_column_int(stmt, 3);
            expenses.push_back(e);
        }
    } else {
        std::cerr << "Could not query table " << tableName << ". It might not exist yet." << std::endl;
    }
    sqlite3_finalize(stmt);
    return expenses;
}

MonthlySummary FinanceDB::getCurrentMonthSummary() {
    MonthlySummary summary = {}; // Zero-initialize
    std::string sql = "SELECT Salary, LimitAmount FROM Overall WHERE month_year = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(mainDB, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, currentYearMonth.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            summary.salary = sqlite3_column_double(stmt, 0);
            summary.limit = sqlite3_column_double(stmt, 1);
        }
    }
    sqlite3_finalize(stmt);
    return summary;
}

double FinanceDB::calcTotalSpent() {
    double total=0.0;
    std::string sql = "SELECT SUM(Price) FROM "+currentTableName+";";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(detailedDB, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            total=sqlite3_column_double(stmt,0);
        }
    } else {
        std::cerr << "Failed to prepare statement for calcTotalSpent: " << sqlite3_errmsg(detailedDB) << std::endl;
    }
    sqlite3_finalize(stmt);
    return total;
}
