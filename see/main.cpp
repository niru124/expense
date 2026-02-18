#include <sodium.h>
#include <sqlite3.h>
#include "./crow_all.h"
#include <iostream>
#include <string>
#include <map>
#include <ctime>

sqlite3* db;

const int SESSION_EXPIRE_SECONDS = 60;  // 1 minute

std::map<std::string, std::pair<std::string, time_t>> sessions;  // token -> {username, expiry}

std::string generate_session_token() {
    unsigned char token[32];
    randombytes_buf(token, sizeof token);
    char hex[65];
    sodium_bin2hex(hex, sizeof hex, token, sizeof token);
    return std::string(hex);
}

std::string get_session_user(const crow::request& req) {
    std::string cookies = req.get_header_value("cookie");
    if (cookies.empty()) return "";
    
    size_t pos = cookies.find("session=");
    if (pos == std::string::npos) return "";
    pos += 8;
    size_t end = cookies.find(";", pos);
    if (end == std::string::npos) end = cookies.length();
    std::string token = cookies.substr(pos, end - pos);
    
    auto it = sessions.find(token);
    if (it == sessions.end()) {
        std::cerr << "Session not found" << std::endl;
        return "";
    }
    
    time_t now = std::time(nullptr);
    std::cerr << "Session expires at " << it->second.second << ", now: " << now << std::endl;
    
    if (now > it->second.second) {
        sessions.erase(token);
        std::cerr << "Session expired" << std::endl;
        return "";
    }
    
    return it->second.first;
}

bool init_database() {
    char* err_msg = nullptr;
    const char* sql = "CREATE TABLE IF NOT EXISTS users ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "username TEXT UNIQUE NOT NULL,"
                      "password_hash TEXT NOT NULL"
                      ");";
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) { std::cerr << err_msg << std::endl; sqlite3_free(err_msg); return false; }
    return true;
}

bool register_user(const std::string& username, const std::string& password) {
    if (username.length() < 3 || username.length() > 32) return false;
    if (password.length() < 6) return false;
    
    char hashed[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hashed, password.c_str(), password.length(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) return false;
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, "INSERT INTO users (username, password_hash) VALUES (?, ?)", -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hashed, -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool verify_login(const std::string& username, const std::string& password) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, "SELECT password_hash FROM users WHERE username = ?", -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) { sqlite3_finalize(stmt); return false; }
    const char* stored = (const char*)sqlite3_column_text(stmt, 0);
    bool ok = (crypto_pwhash_str_verify(stored, password.c_str(), password.length()) == 0);
    sqlite3_finalize(stmt);
    return ok;
}

int main() {
    if (sodium_init() < 0) return 1;
    if (sqlite3_open("users.db", &db) != SQLITE_OK) return 1;
    if (!init_database()) return 1;
    
    crow::SimpleApp app;
    
    CROW_ROUTE(app, "/")([]{ return "API: POST /register, /login | Protected: GET /me | Session expires in 60s"; });
    
    CROW_ROUTE(app, "/register").methods(crow::HTTPMethod::POST)([](const crow::request& req) {
        auto b = crow::json::load(req.body);
        if (!b || !b.has("username") || !b.has("password")) return crow::response(400, "{\"error\": \"Missing fields\"}");
        std::string u = b["username"].s(), p = b["password"].s();
        if (register_user(u, p)) return crow::response(200, "{\"ok\": true}");
        return crow::response(400, "{\"error\": \"Failed\"}");
    });
    
    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST)([](const crow::request& req) {
        auto b = crow::json::load(req.body);
        if (!b || !b.has("username") || !b.has("password")) return crow::response(400, "{\"error\": \"Missing fields\"}");
        std::string u = b["username"].s(), p = b["password"].s();
        if (!verify_login(u, p)) return crow::response(401, "{\"error\": \"Invalid credentials\"}");
        
        std::string token = generate_session_token();
        time_t expiry = std::time(nullptr) + SESSION_EXPIRE_SECONDS;
        sessions[token] = {u, expiry};
        
        crow::response res(200, "{\"user\": \"" + u + "\", \"expires_in\": " + std::to_string(SESSION_EXPIRE_SECONDS) + "}");
        res.add_header("Set-Cookie", "session=" + token + "; Path=/");
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
            sessions.erase(token);
        }
        crow::response res(200, "{\"ok\": true}");
        res.add_header("Set-Cookie", "session=; Path=/; Max-Age=0");
        return res;
    });
    
    CROW_ROUTE(app, "/me")([&](const crow::request& req) {
        std::string user = get_session_user(req);
        if (user.empty()) return crow::response(401, "{\"error\": \"Unauthorized or expired\"}");
        return crow::response(200, "{\"user\": \"" + user + "\"}");
    });
    
    std::cout << "Server: http://localhost:18080 | Session expiry: " << SESSION_EXPIRE_SECONDS << " seconds\n";
    app.port(18080).run();
    sqlite3_close(db);
    return 0;
}
