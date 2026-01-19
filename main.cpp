#include "FinanceDB.h"
#include "crow.h"
#include "helper.h"
#include "crow/middlewares/cors.h"
#include <sciplot/sciplot.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <memory>
#include <sstream>

int main() {
  auto db_ptr = std::make_shared<FinanceDB>("Main.db", "Detailed.db");

  crow::App<crow::CORSHandler> app;

  CROW_ROUTE(app, "/")([]() {
    return "<p>........working.....</p>"
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
          response[i]["id"] = expenses[i].id;
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

        if (!isNumber(salary) || !isNumber(limit)) {
          return crow::response(400, "Bad Request: 'salary' and 'limit' must be numbers.");
        }

        if (db_ptr->addOrUpdateMonthlySummary(salary, limit)) {
          return crow::response(200, "Monthly summary updated.");
        }
        return crow::response(500, "Failed to update summary.");
      });

  CROW_ROUTE(app, "/expense")
      .methods(crow::HTTPMethod::Post)([&db_ptr](const crow::request &req) {
        auto data = crow::json::load(req.body);
        if (!data || !data.has("spentOn") || !data.has("price")) {
          return crow::response(400,
                                "Bad Request: Missing 'spentOn' or 'price'.");
        }

        std::string spentOn = refinedString(data["spentOn"].s());
        double price = data["price"].d();

        if (!isNumber(price)) {
          return crow::response(400, "Bad Request: 'price' must be a number.");
        }

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
      response[i]["average_price"] =
          prioritizedExpenses[i]
              .price;
      response[i]["times_purchased"] =
          prioritizedExpenses[i].priority;
    }
    return crow::response(response);
  });

  CROW_ROUTE(app, "/range/<string>/<string>")
      .methods(
          crow::HTTPMethod::Get)([&db_ptr](const std::string &start_date_str,
                                           const std::string &end_date_str) {
        if (start_date_str.empty() || end_date_str.empty()) {
          return crow::response(
              crow::status::BAD_REQUEST,
              "Bad Request: Both start_date and end_date are required.");
        }

        std::string formatted_start_date = format_date(start_date_str);
        std::string formatted_end_date = format_date(end_date_str);

        if (formatted_start_date.empty() || formatted_end_date.empty()) {
          return crow::response(
              crow::status::BAD_REQUEST,
              "Bad Request: Invalid date format. Use DD-MM-YYYY.");
        }

        auto rangedExpenses =
            db_ptr->getRangeOfDate(formatted_start_date, formatted_end_date);
        crow::json::wvalue response;
        for (size_t i = 0; i < rangedExpenses.size(); ++i) {
          response[i]["id"] = rangedExpenses[i].id;
          response[i]["day_month_year"] = rangedExpenses[i].day_month_year;
          response[i]["spent_on"] = rangedExpenses[i].spent_on;
          response[i]["price"] = rangedExpenses[i].price;
          response[i]["priority"] = rangedExpenses[i].priority;
        }
        return crow::response(response);
      });

  CROW_ROUTE(app, "/sorted_by_price/<string>")
      .methods(crow::HTTPMethod::Get)([&db_ptr](const std::string &order_str) {
        bool increasing = true;
        if (order_str == "false") {
          increasing = false;
        } else if (order_str != "true" && !order_str.empty()) {
          return crow::response(
              crow::status::BAD_REQUEST,
              "Invalid order parameter. Use 'true' for ascending, "
              "'false' for descending, or leave empty for ascending.");
        }

        auto sortedExpenses = db_ptr->calcSortByPrice(increasing);
        crow::json::wvalue response;
        for (size_t i = 0; i < sortedExpenses.size(); ++i) {
          response[i]["id"] = sortedExpenses[i].id;
          response[i]["day_month_year"] = sortedExpenses[i].day_month_year;
          response[i]["spent_on"] = sortedExpenses[i].spent_on;
          response[i]["price"] = sortedExpenses[i].price;
          response[i]["priority"] = sortedExpenses[i].priority;
        }
        return crow::response(response);
      });

  CROW_ROUTE(app, "/sorted_by_price/")
      .methods(crow::HTTPMethod::Get)([&db_ptr]() {
        auto sortedExpenses = db_ptr->calcSortByPrice(true);
        crow::json::wvalue response;
        for (size_t i = 0; i < sortedExpenses.size(); ++i) {
          response[i]["id"] = sortedExpenses[i].id;
          response[i]["day_month_year"] = sortedExpenses[i].day_month_year;
          response[i]["spent_on"] = sortedExpenses[i].spent_on;
          response[i]["price"] = sortedExpenses[i].price;
          response[i]["priority"] = sortedExpenses[i].priority;
        }
        return crow::response(response);
      });

  CROW_ROUTE(app, "/total_spent").methods(crow::HTTPMethod::Get)([&db_ptr]() {
    double totalSpentAmount = db_ptr->calcTotalSpent();
    crow::json::wvalue response;
    response["total"] = totalSpentAmount;
    return crow::response(response);
  });

  CROW_ROUTE(app, "/delete_expense/<int>").methods(crow::HTTPMethod::Delete)([&db_ptr](int id) {
    if (!isNumber(id)) {
      return crow::response(400, "Bad Request: ID must be a number.");
    }
    if (db_ptr->deleteSelected(id)) {
      return crow::response(200, "Expense with ID " + std::to_string(id) + " deleted successfully.");
    } else {
      return crow::response(500, "Failed to delete expense with ID " + std::to_string(id) + ".");
    }
  });

  CROW_ROUTE(app, "/edit_expense/<int>")
      .methods(crow::HTTPMethod::Put)([&db_ptr](const crow::request &req, int id) {
        auto data = crow::json::load(req.body);
        if (!data) {
          return crow::response(400, "Bad Request: Invalid JSON.");
        }

        std::optional<std::string> spentOn;
        if (data.has("spentOn")) {
          spentOn = refinedString(data["spentOn"].s());
        }

        std::optional<double> price;
        if (data.has("price")) {
          if (!isNumber(data["price"].d())) {
            return crow::response(400, "Bad Request: 'price' must be a number.");
          }
          price = data["price"].d();
        }

        std::optional<int> priority;
        if (data.has("priority")) {
          if (!isNumber(data["priority"].i())) {
            return crow::response(400, "Bad Request: 'priority' must be an integer.");
          }
          priority = data["priority"].i();
        }

        if (!spentOn && !price && !priority) {
          return crow::response(400, "Bad Request: No fields provided for update.");
        }

        if (db_ptr->updateSelected2(id, spentOn, price, priority)) {
          return crow::response(200, "Expense with ID " + std::to_string(id) + " updated successfully.");
        } else {
          return crow::response(500, "Failed to update expense with ID " + std::to_string(id) + ".");
        }
      });

  // auto detect current year
  CROW_ROUTE(app, "/graph/yearly")
      .methods(crow::HTTPMethod::Get)([&db_ptr]() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm* tm_local = std::localtime(&now_time);
        int year = tm_local->tm_year + 1900;

        auto monthlyTotals = db_ptr->getMonthlyTotalsForYear(year);

        std::vector<std::string> months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        std::vector<double> values;
        for (const auto& month : months) {
            values.push_back(monthlyTotals[month]);
        }

        using namespace sciplot;
        Plot2D plot;
        plot.drawHistogram(values).label("Monthly Expenses for " + std::to_string(year));
        plot.xlabel("Month");
        plot.ylabel("Amount Spent");
        plot.grid().show();

        std::string xticLabels = "\"Jan\" 0, \"Feb\" 1, \"Mar\" 2, \"Apr\" 3, \"May\" 4, \"Jun\" 5, \"Jul\" 6, \"Aug\" 7, \"Sep\" 8, \"Oct\" 9, \"Nov\" 10, \"Dec\" 11";
        plot.gnuplot("set xtics (" + xticLabels + ")");

        Figure figure = {{plot}};
        Canvas canvas = {{figure}};
        canvas.size(800, 600);
        canvas.title("Yearly Expense Report - " + std::to_string(year));

        std::string tempFile = "/tmp/yearly_expense_graph_" + std::to_string(year) + ".svg";
        canvas.save(tempFile);

        std::ifstream file(tempFile);
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string svgContent = buffer.str();
        file.close();
        std::remove(tempFile.c_str());

        crow::response response;
        response.set_header("Content-Type", "image/svg+xml");
        response.body = svgContent;
        return response;
      });

  CROW_ROUTE(app, "/graph/yearly/<int>")
      .methods(crow::HTTPMethod::Get)([&db_ptr](int year) {
        auto monthlyTotals = db_ptr->getMonthlyTotalsForYear(year);

        std::vector<std::string> months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        std::vector<double> values;
        for (const auto& month : months) {
            values.push_back(monthlyTotals[month]);
        }

        using namespace sciplot;
        Plot2D plot;
        plot.drawHistogram(values).label("Monthly Expenses for " + std::to_string(year));
        plot.xlabel("Month");
        plot.ylabel("Amount Spent");
        plot.grid().show();

        std::string xticLabels = "\"Jan\" 0, \"Feb\" 1, \"Mar\" 2, \"Apr\" 3, \"May\" 4, \"Jun\" 5, \"Jul\" 6, \"Aug\" 7, \"Sep\" 8, \"Oct\" 9, \"Nov\" 10, \"Dec\" 11";
        plot.gnuplot("set xtics (" + xticLabels + ")");

        Figure figure = {{plot}};
        Canvas canvas = {{figure}};
        canvas.size(800, 600);
        canvas.title("Yearly Expense Report - " + std::to_string(year));

        std::string tempFile = "/tmp/yearly_expense_graph_" + std::to_string(year) + ".svg";
        canvas.save(tempFile);

        std::ifstream file(tempFile);
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string svgContent = buffer.str();
        file.close();
        std::remove(tempFile.c_str());

        crow::response response;
        response.set_header("Content-Type", "image/svg+xml");
        response.body = svgContent;
        return response;
      });

  std::cout << "Starting server on port 5000..." << std::endl;

  app.port(5000).run();

  return 0;
}
