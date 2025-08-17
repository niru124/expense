#include "FinanceDB.h"
#include "crow.h"
#include <memory>

int main() {
  auto db_ptr = std::make_shared<FinanceDB>("Main.db", "Detailed.db");

  crow::SimpleApp app;

  CROW_ROUTE(app, "/")([]() {
    return "........working......"
           "<div><a href='/summary'>View All Summaries</a></div>"
           "<div><a href='/expenses/08_2025'>View Expenses for August 2025 "
           "(example)</a></div>";
  });

  CROW_ROUTE(app, "/summary").methods(crow::HTTPMethod::Get)([&db_ptr]() {
    auto summaries = db_ptr->getAllSummaries();
    crow::json::wvalue response;
    for (size_t i = 0; i < summaries.size(); ++i) {
      response[i]["month_year"] = summaries[i].month_year;
      response[i]["salary"] = summaries[i].salary;
      response[i]["limit"] = summaries[i].limit;
      response[i]["saving_percentage"] = summaries[i].saving_percentage;
      response[i]["condition"] = summaries[i].condition;
    }
    return crow::response(response);
  });

  CROW_ROUTE(app, "/expenses/<string>")
      .methods(crow::HTTPMethod::Get)([&db_ptr](const std::string &month_year) {
        auto expenses = db_ptr->getExpensesForMonth(month_year);
        crow::json::wvalue response;
        for (size_t i = 0; i < expenses.size(); ++i) {
          response[i]["day_month_year"] = expenses[i].day_month_year;
          response[i]["spent_on"] = expenses[i].spent_on;
          response[i]["price"] = expenses[i].price;
          response[i]["priority"] = expenses[i].priority;
        }
        return crow::response(response);
      });

  CROW_ROUTE(app, "/summary")
      .methods(crow::HTTPMethod::Post)([&db_ptr](const crow::request &req) {
        auto data = crow::json::load(req.body);
        if (!data || !data.has("salary") || !data.has("limit")) {
          return crow::response(400,
                                "Bad Request: Missing 'salary' or 'limit'.");
        }

        double salary = data["salary"].d();
        double limit = data["limit"].d();

        if (db_ptr->addOrUpdateMonthlySummary(salary, limit)) {
          return crow::response(200, "Monthly summary updated.");
        }
        return crow::response(500, "Failed to update summary.");
      });

  CROW_ROUTE(app, "/expense")
      .methods(crow::HTTPMethod::Post)([&db_ptr](const crow::request &req) {
        auto data = crow::json::load(req.body);
        if (!data || !data.has("spentOn") || !data.has("price")) {
          return crow::response(
              400, "Bad Request: Missing 'spentOn' or 'price'.");
        }

        std::string spentOn = data["spentOn"].s();
        double price = data["price"].d();
        // Priority is handled internally by addExpense and updatePriority

        if (!db_ptr->addExpense(spentOn, price)) {
          return crow::response(500, "Failed to add expense.");
        }

        auto current_summary = db_ptr->getCurrentMonthSummary();
        if (current_summary.salary > 0) {
          db_ptr->addOrUpdateMonthlySummary(current_summary.salary,
                                            current_summary.limit);
        } else {
          std::cout << "Expense added, but summary not updated because salary "
                       "is not set for the month."
                    << std::endl;
        }

        return crow::response(200, "Expense added successfully.");
      });

  CROW_ROUTE(app, "/highest").methods(crow::HTTPMethod::Get)([&db_ptr]() {
    auto prioritizedExpenses = db_ptr->calcPriority();
    crow::json::wvalue response;
    for (size_t i = 0; i < prioritizedExpenses.size(); ++i) {
      response[i]["spent_on"] = prioritizedExpenses[i].spent_on;
      response[i]["average_price"] = prioritizedExpenses[i].price; // price in this context is average price
      response[i]["times_purchased"] = prioritizedExpenses[i].priority; // priority in this context is count
    }
    return crow::response(response);
  });

  std::cout << "Starting server on port 5000..." << std::endl;
  app.port(5000).multithreaded().run();
  
  return 0;
}
