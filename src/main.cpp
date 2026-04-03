#include "FinanceDB.h"
#include "crow_all.h"
#include "helper.h"
#include <sqlite3.h>
#include <sodium.h>
#include <sciplot/sciplot.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <memory>
#include <algorithm>
#include <string>
#include <map>
#include <mutex>
#include <iostream>

extern std::map<std::string, std::pair<int, time_t>> sessions;
extern std::mutex sessions_mutex;

struct AuthMiddleware {
    struct context {
        int user_id = -1;
    };

    static std::string get_cookie(const crow::request& req, const std::string& name) {
        std::string cookies = req.get_header_value("cookie");
        if (cookies.empty()) return "";
        
        size_t pos = cookies.find(name + "=");
        if (pos == std::string::npos) return "";
        pos += name.length() + 1;
        size_t end = cookies.find(";", pos);
        if (end == std::string::npos) end = cookies.length();
        return cookies.substr(pos, end - pos);
    }

    static bool is_public_route(const std::string& path) {
        return path == "/" || 
               path == "/register" || 
               path == "/login" || 
               path == "/logout";
    }

    void before_handle(crow::request& req, crow::response& res, context& ctx) {
        if (is_public_route(req.url)) {
            return;
        }
        
        std::string token = get_cookie(req, "session");
        if (token.empty()) {
            res.code = 401;
            res.body = "{\"error\": \"Unauthorized\"}";
            res.set_header("Content-Type", "application/json");
            return;
        }
        
        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto it = sessions.find(token);
        if (it == sessions.end()) {
            res.code = 401;
            res.body = "{\"error\": \"Unauthorized\"}";
            res.set_header("Content-Type", "application/json");
            return;
        }
        
        time_t now = std::time(nullptr);
        if (now > it->second.second) {
            sessions.erase(token);
            res.code = 401;
            res.body = "{\"error\": \"Session expired\"}";
            res.set_header("Content-Type", "application/json");
            return;
        }
        
        ctx.user_id = it->second.first;
    }

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
    }
};

sqlite3* auth_db;

const int SESSION_EXPIRE_SECONDS = 3600;

std::map<std::string, std::pair<int, time_t>> sessions;
std::mutex sessions_mutex;

std::string generate_session_token() {
    unsigned char token[32];
    randombytes_buf(token, sizeof token);
    char hex[65];
    sodium_bin2hex(hex, sizeof hex, token, sizeof token);
    return std::string(hex);
}

int get_session_user_id(const crow::request& req) {
    std::string cookies = req.get_header_value("cookie");
    if (cookies.empty()) return -1;
    
    size_t pos = cookies.find("session=");
    if (pos == std::string::npos) return -1;
    pos += 8;
    size_t end = cookies.find(";", pos);
    if (end == std::string::npos) end = cookies.length();
    std::string token = cookies.substr(pos, end - pos);
    
    std::lock_guard<std::mutex> lock(sessions_mutex);
    auto it = sessions.find(token);
    if (it == sessions.end()) return -1;
    
    time_t now = std::time(nullptr);
    if (now > it->second.second) {
        sessions.erase(token);
        return -1;
    }
    
    return it->second.first;
}

bool init_auth_database() {
    char* err_msg = nullptr;
    const char* sql = "CREATE TABLE IF NOT EXISTS users ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "username TEXT UNIQUE NOT NULL,"
                      "password_hash TEXT NOT NULL"
                      ");"
                      "CREATE TABLE IF NOT EXISTS expenses ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "user_id INTEGER NOT NULL,"
                      "day_month_year TEXT NOT NULL,"
                      "spent_on TEXT NOT NULL,"
                      "price REAL NOT NULL,"
                      "category TEXT,"
                      "priority INTEGER DEFAULT 0,"
                      "FOREIGN KEY(user_id) REFERENCES users(id)"
                      ");"
                      "CREATE TABLE IF NOT EXISTS summaries ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "user_id INTEGER NOT NULL,"
                      "month_year TEXT NOT NULL,"
                      "salary REAL NOT NULL,"
                      "limit_amount REAL NOT NULL,"
                      "saving_percentage REAL,"
                      "condition TEXT,"
                      "UNIQUE(user_id, month_year),"
                      "FOREIGN KEY(user_id) REFERENCES users(id)"
                      ");";
    int rc = sqlite3_exec(auth_db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) { 
        std::cerr << "Auth DB Error: " << err_msg << std::endl; 
        sqlite3_free(err_msg); 
        return false; 
    }
    return true;
}

bool register_user(const std::string& username, const std::string& password) {
    if (username.length() < 3 || username.length() > 32) return false;
    if (password.length() < 6) return false;
    
    char hashed[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hashed, password.c_str(), password.length(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) return false;
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(auth_db, "INSERT INTO users (username, password_hash) VALUES (?, ?)", -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hashed, -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

int verify_login(const std::string& username, const std::string& password) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(auth_db, "SELECT id, password_hash FROM users WHERE username = ?", -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) { sqlite3_finalize(stmt); return -1; }
    int user_id = sqlite3_column_int(stmt, 0);
    const char* stored = (const char*)sqlite3_column_text(stmt, 1);
    bool ok = (crypto_pwhash_str_verify(stored, password.c_str(), password.length()) == 0);
    sqlite3_finalize(stmt);
    return ok ? user_id : -1;
}

std::string getCurrentMonthYearStr() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_local;
    localtime_r(&now_time, &tm_local);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%m_%Y", &tm_local);
    return std::string(buffer);
}

int main() {
  if (sodium_init() < 0) {
    std::cerr << "Failed to initialize libsodium" << std::endl;
    return 1;
  }
  if (sqlite3_open("auth.db", &auth_db) != SQLITE_OK) {
    std::cerr << "Failed to open auth database" << std::endl;
    return 1;
  }
  if (!init_auth_database()) {
    std::cerr << "Failed to initialize auth database" << std::endl;
    return 1;
  }

  auto db_ptr = std::make_shared<FinanceDB>("Main.db", "Detailed.db");

  crow::App<crow::CORSHandler, AuthMiddleware> app;
  
  app.get_middleware<crow::CORSHandler>().global().allow_credentials();

  CROW_ROUTE(app, "/")([]{ return "<p>Expense Tracker API</p>"
         "<div><a href='/summary'>View All Summaries</a></div>"
         "<div><a href='/expenses/08_2025'>View Expenses for August 2025 (example)</a></div>"
         "<div><a href='/register'>Register</a></div>"
         "<div><a href='/login'>Login</a></div>"
         "<div><a href='http://localhost:8088'>Go to Frontend App</a></div>"; });

  CROW_ROUTE(app, "/register").methods(crow::HTTPMethod::Post)([](const crow::request& req) {
      auto b = crow::json::load(req.body);
      if (!b || !b.has("username") || !b.has("password")) {
        return crow::response(400, "{\"error\": \"Missing username or password\"}");
      }
      std::string u = b["username"].s();
      std::string p = b["password"].s();
      if (register_user(u, p)) {
        return crow::response(200, "{\"ok\": true, \"message\": \"User registered successfully\"}");
      }
      return crow::response(400, "{\"error\": \"Registration failed - username may be taken or invalid\"}");
  });

  CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST)([](const crow::request& req) {
      auto b = crow::json::load(req.body);
      if (!b || !b.has("username") || !b.has("password")) {
        return crow::response(400, "{\"error\": \"Missing username or password\"}");
      }
      std::string u = b["username"].s();
      std::string p = b["password"].s();
      int user_id = verify_login(u, p);
      if (user_id < 0) {
        return crow::response(401, "{\"error\": \"Invalid credentials\"}");
      }
      
      std::string token = generate_session_token();
      time_t expiry = std::time(nullptr) + SESSION_EXPIRE_SECONDS;
      {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        sessions[token] = {user_id, expiry};
      }
      
      crow::response res(200, "{\"user_id\": " + std::to_string(user_id) + ", \"username\": \"" + u + "\", \"expires_in\": " + std::to_string(SESSION_EXPIRE_SECONDS) + "}");
      res.add_header("Set-Cookie", "session=" + token + "; Path=/; HttpOnly");
      return res;
  });

  CROW_ROUTE(app, "/logout").methods(crow::HTTPMethod::POST)([](const crow::request& req) {
      std::string cookies = req.get_header_value("cookie");
      size_t pos = cookies.find("session=");
      if (pos != std::string::npos) {
          pos += 8;
          size_t end = cookies.find(";", pos);
          if (end == std::string::npos) end = cookies.length();
          std::string token = cookies.substr(pos, end - pos);
          std::lock_guard<std::mutex> lock(sessions_mutex);
          sessions.erase(token);
      }
      crow::response res(200, "{\"ok\": true}");
      res.add_header("Set-Cookie", "session=; Path=/; Max-Age=0");
      return res;
  });

  CROW_ROUTE(app, "/me").methods(crow::HTTPMethod::GET)([](const crow::request& req) {
      int user_id = get_session_user_id(req);
      if (user_id < 0) {
        return crow::response(401, "{\"error\": \"Unauthorized\"}");
      }
      
      sqlite3_stmt* stmt;
      if (sqlite3_prepare_v2(auth_db, "SELECT username FROM users WHERE id = ?", -1, &stmt, nullptr) != SQLITE_OK) {
        return crow::response(500, "{\"error\": \"Database error\"}");
      }
      sqlite3_bind_int(stmt, 1, user_id);
      std::string username;
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        username = (const char*)sqlite3_column_text(stmt, 0);
      }
      sqlite3_finalize(stmt);
      
      return crow::response(200, "{\"user_id\": " + std::to_string(user_id) + ", \"username\": \"" + username + "\"}");
  });

  CROW_ROUTE(app, "/summary").methods(crow::HTTPMethod::Get)([&db_ptr](const crow::request& req) {
    int user_id = get_session_user_id(req);
    if (user_id < 0) return crow::response(401, "{\"error\": \"Unauthorized\"}");
    
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
          response[i]["category"] = expenses[i].category;
          response[i]["mode_of_payment"] = expenses[i].mode_of_payment;
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
      .methods(crow::HTTPMethod::POST)([&db_ptr](const crow::request &req) {
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

        std::optional<std::string> category = std::nullopt;
        if (data.has("category")) {
          category = refinedString(data["category"].s());
          if (category->empty()) {
            category = std::nullopt;
          }
        }

        std::optional<std::string> date = std::nullopt;
        if (data.has("date")) {
          date = refinedString(data["date"].s());
          if (date->empty()) {
            date = std::nullopt;
          }
        }

        std::optional<std::string> modeOfPayment = std::nullopt;
        if (data.has("modeOfPayment")) {
          modeOfPayment = refinedString(data["modeOfPayment"].s());
          if (modeOfPayment->empty()) {
            modeOfPayment = std::nullopt;
          }
        }

        if (!db_ptr->addExpense(spentOn, price, category, date, modeOfPayment)) {
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
          response[i]["category"] = rangedExpenses[i].category;
          response[i]["mode_of_payment"] = rangedExpenses[i].mode_of_payment;
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
          response[i]["category"] = sortedExpenses[i].category;
          response[i]["mode_of_payment"] = sortedExpenses[i].mode_of_payment;
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
          response[i]["category"] = sortedExpenses[i].category;
          response[i]["mode_of_payment"] = sortedExpenses[i].mode_of_payment;
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

  CROW_ROUTE(app, "/categories").methods(crow::HTTPMethod::Get)([&db_ptr]() {
    auto categories = db_ptr->getAllCategories();
    
    crow::json::wvalue response;
    for (size_t i = 0; i < categories.size(); ++i) {
      response[i] = categories[i];
    }
    return crow::response(response);
  });

  CROW_ROUTE(app, "/add_category").methods(crow::HTTPMethod::Post)([&db_ptr](const crow::request &req) {
    auto data = crow::json::load(req.body);
    if (!data || !data.has("category")) {
      return crow::response(400, "Bad Request: Missing 'category'.");
    }
    std::string category = refinedString(data["category"].s());
    if (db_ptr->addCategory(category)) {
      return crow::response(200, "Category added successfully.");
    }
    return crow::response(500, "Failed to add category.");
  });

  CROW_ROUTE(app, "/mode_of_payment").methods(crow::HTTPMethod::Get)([&db_ptr]() {
    auto modes = db_ptr->getAllModeOfPayment();
    
    crow::json::wvalue response;
    for (size_t i = 0; i < modes.size(); ++i) {
      response[i] = modes[i];
    }
    return crow::response(response);
  });

  CROW_ROUTE(app, "/add_mode_of_payment").methods(crow::HTTPMethod::Post)([&db_ptr](const crow::request &req) {
    auto data = crow::json::load(req.body);
    if (!data || !data.has("modeOfPayment")) {
      return crow::response(400, "Bad Request: Missing 'modeOfPayment'.");
    }
    std::string modeOfPayment = refinedString(data["modeOfPayment"].s());
    if (db_ptr->addModeOfPayment(modeOfPayment)) {
      return crow::response(200, "Mode of payment added successfully.");
    }
    return crow::response(500, "Failed to add mode of payment.");
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

        std::optional<std::string> category;
        if (data.has("category")) {
          category = refinedString(data["category"].s());
        }

        std::optional<std::string> modeOfPayment;
        if (data.has("modeOfPayment")) {
          modeOfPayment = refinedString(data["modeOfPayment"].s());
        }

        std::optional<std::string> date;
        if (data.has("date")) {
          date = refinedString(data["date"].s());
        }

        std::optional<int> priority;
        if (data.has("priority")) {
          if (!isNumber(data["priority"].i())) {
            return crow::response(400, "Bad Request: 'priority' must be an integer.");
          }
          priority = data["priority"].i();
        }

        if (!spentOn && !price && !category && !modeOfPayment && !date && !priority) {
          return crow::response(400, "Bad Request: No fields provided for update.");
        }

        if (db_ptr->updateSelected3(id, spentOn, price, category, modeOfPayment, date, priority)) {
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
