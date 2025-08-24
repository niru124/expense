#ifndef FINANCEDB_H
#define FINANCEDB_H

#include <sqlite3.h>
#include <string>
#include <vector>

// Struct to hold a record from the 'Overall' table
struct MonthlySummary {
  std::string month_year;
  double salary;
  double limit;
  double saving_percentage;
  std::string condition;
};

// Struct to hold a record from a detailed expense table
struct ExpenseRecord {
  std::string day_month_year;
  std::string spent_on;
  double price;
  int priority;
};

class FinanceDB {
private:
  sqlite3 *mainDB;
  sqlite3 *detailedDB;
  std::string currentYearMonth;
  std::string currentTableName;

  void initMainDB();
  void initDetailedDB();
  void executeSQL(sqlite3 *db, const std::string &sql);

  double calculateCurrentSavings(double salary);
  std::string determineCondition(double savingPercentage);

public:
  // Constructor and Destructor
  FinanceDB(const std::string &mainDbPath, const std::string &detailedDbPath);
  ~FinanceDB();

  // --- Methods for Adding Data ---
  bool addOrUpdateMonthlySummary(double salary, double limit);
  bool addExpense(const std::string &spentOn, double price);
  void updatePriority(const std::string &spentOn);

  // --- New Methods for Viewing Data ---
  std::vector<MonthlySummary> getAllSummaries();
  std::vector<ExpenseRecord>
  getExpensesForMonth(const std::string &monthYear); // e.g., "08_2025"
  MonthlySummary getCurrentMonthSummary();

  std::vector<ExpenseRecord> getSortedByVal();
  std::vector<ExpenseRecord> calcPriority();
  std::vector<ExpenseRecord> calcSortByPrice(bool order);
  std::vector<ExpenseRecord> getRangeOfDate(std::string start_date,
                                            std::string end_date);
  double calcTotalSpent();
  bool deleteSelected(int id);
};

#endif // FINANCEDB_H
