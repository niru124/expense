#ifndef FINANCEDB_H
#define FINANCEDB_H

#include <map>
#include <optional>
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
  int id; // Corresponds to SQLite rowid
  std::string day_month_year;
  std::string spent_on;
  double price;
  std::string category;
  std::string mode_of_payment;
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
  bool addExpense(const std::string &spentOn, double price, const std::optional<std::string> &category = std::nullopt, const std::optional<std::string> &date = std::nullopt, const std::optional<std::string> &modeOfPayment = std::nullopt);
  void updatePriority(const std::string &spentOn);

  // --- Methods for Categories and Mode of Payment ---
  std::vector<std::string> getAllCategories();
  bool addCategory(const std::string &category);
  std::vector<std::string> getAllModeOfPayment();
  bool addModeOfPayment(const std::string &modeOfPayment);

  // --- Methods for Viewing Data ---
  std::vector<MonthlySummary> getAllSummaries();
  std::vector<ExpenseRecord> getExpensesForMonth(const std::string &monthYear);
  MonthlySummary getCurrentMonthSummary();

  std::vector<ExpenseRecord> getSortedByVal();
  std::vector<ExpenseRecord> calcPriority();
  std::vector<ExpenseRecord> calcSortByPrice(bool order);
  std::vector<ExpenseRecord> getRangeOfDate(std::string start_date, std::string end_date);
  std::vector<ExpenseRecord> getItemByDateRange(std::string item, std::string start_date, std::string end_date);
  std::map<std::string, double> getMonthlyTotalsForYear(int year);
  double calcTotalSpent();
  bool deleteSelected(int id);

  bool updateSelected2(int id, const std::optional<std::string> &spentOn,
                      const std::optional<double> &price,
                      const std::optional<int> &priority);

  bool updateSelected3(int id, const std::optional<std::string> &spentOn,
                      const std::optional<double> &price,
                      const std::optional<std::string> &category,
                      const std::optional<std::string> &modeOfPayment,
                      const std::optional<std::string> &date,
                      const std::optional<int> &priority);
};

#endif // FINANCEDB_H
