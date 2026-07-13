#ifndef JSONSTORE_DATABASE_HPP
#define JSONSTORE_DATABASE_HPP

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <format>
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace jsonstore {

struct Error {
  int code;
  std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

class Database {
public:
  explicit Database(std::string_view path) : impl_(std::make_unique<Impl>()) {
    int rc = sqlite3_open(path.data(), &impl_->db);
    if (rc != SQLITE_OK) {
      auto msg = std::format("Failed to open database: {}", sqlite3_errmsg(impl_->db));
      sqlite3_close(impl_->db);
      impl_->db = nullptr;
      throw std::runtime_error(msg);
    }
    execute("PRAGMA journal_mode=WAL");
    execute("PRAGMA busy_timeout=5000");
    init_schema();
    spdlog::info("Opened database: {}", path);
  }

  ~Database() {
    if (impl_ && impl_->db) {
      sqlite3_close(impl_->db);
      spdlog::info("Database closed");
    }
  }

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&&) noexcept = default;
  Database& operator=(Database&&) noexcept = default;

  /// Store a JSON document with a key
  Result<long long> put(const std::string& key, const json& doc) {
    auto sql = std::format("INSERT OR REPLACE INTO documents (key, value, created_at) "
                          "VALUES ('{}', '{}', datetime('now'))",
                          escape(key), escape(doc.dump()));
    return insert(sql);
  }

  /// Retrieve a JSON document by key
  Result<json> get(const std::string& key) {
    auto sql = std::format("SELECT key, value, created_at, updated_at FROM documents WHERE key = '{}'", escape(key));
    auto rows = query(sql);
    if (!rows) return std::unexpected(rows.error());
    if (rows->empty()) return std::unexpected(Error{404, "Document not found: " + key});
    return (*rows)[0];
  }

  /// Delete a document by key
  Result<int> remove(const std::string& key) {
    auto sql = std::format("DELETE FROM documents WHERE key = '{}'", escape(key));
    return execute(sql);
  }

  /// List all document keys (with optional prefix filter)
  Result<std::vector<json>> list(const std::string& prefix = "") {
    std::string sql;
    if (prefix.empty()) {
      sql = "SELECT key, value, created_at, updated_at FROM documents ORDER BY key";
    } else {
      sql = std::format("SELECT key, value, created_at, updated_at FROM documents "
                        "WHERE key LIKE '{}%' ORDER BY key", escape(prefix));
    }
    return query(sql);
  }

  /// Search documents by JSON path expression (simple contains)
  Result<std::vector<json>> search(const std::string& search_term) {
    auto sql = std::format("SELECT key, value, created_at, updated_at FROM documents "
                          "WHERE value LIKE '%{}%' OR key LIKE '%{}%' ORDER BY key",
                          escape(search_term), escape(search_term));
    return query(sql);
  }

  /// Count documents
  Result<int> count() {
    auto sql = "SELECT COUNT(*) as count FROM documents";
    auto rows = query(sql);
    if (!rows) return std::unexpected(rows.error());
    if (rows->empty()) return 0;
    return (*rows)[0]["count"].get<int>();
  }

  /// Export all documents as JSON array
  Result<json> export_all() {
    auto rows = list();
    if (!rows) return std::unexpected(rows.error());
    json result = json::array();
    for (auto& row : *rows) {
      result.push_back(row);
    }
    return result;
  }

  /// Import documents from JSON array
  Result<int> import_json(const json& docs) {
    if (!docs.is_array()) return std::unexpected(Error{400, "Expected JSON array"});
    int count = 0;
    for (const auto& doc : docs) {
      if (!doc.contains("key") || !doc.contains("value")) continue;
      auto key = doc["key"].get<std::string>();
      auto value = doc["value"];
      auto result = put(key, value);
      if (result) count++;
    }
    return count;
  }

private:
  struct Impl { sqlite3* db = nullptr; };
  std::unique_ptr<Impl> impl_;

  void init_schema() {
    execute("CREATE TABLE IF NOT EXISTS documents ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  key TEXT UNIQUE NOT NULL,"
            "  value TEXT NOT NULL,"
            "  created_at TEXT DEFAULT (datetime('now')),"
            "  updated_at TEXT DEFAULT (datetime('now'))"
            ")");
    execute("CREATE INDEX IF NOT EXISTS idx_documents_key ON documents(key)");
    spdlog::info("Schema initialized");
  }

  std::string escape(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
      if (c == '\'') result += "''";
      else result += c;
    }
    return result;
  }

  Result<std::vector<json>> query(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    if (int rc = sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr);
        rc != SQLITE_OK) {
      return std::unexpected(Error{500, sqlite3_errmsg(impl_->db)});
    }

    std::vector<json> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      json row;
      int cols = sqlite3_column_count(stmt);
      for (int i = 0; i < cols; ++i) {
        std::string name = sqlite3_column_name(stmt, i);
        switch (sqlite3_column_type(stmt, i)) {
          case SQLITE_INTEGER: row[name] = sqlite3_column_int64(stmt, i); break;
          case SQLITE_FLOAT:   row[name] = sqlite3_column_double(stmt, i); break;
          case SQLITE_TEXT:    row[name] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i)); break;
          case SQLITE_NULL:    row[name] = nullptr; break;
          default:             row[name] = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, i)); break;
        }
      }
      rows.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return rows;
  }

  Result<int> execute(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(impl_->db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
      std::string msg = err_msg ? err_msg : "Unknown error";
      sqlite3_free(err_msg);
      return std::unexpected(Error{500, std::move(msg)});
    }
    return sqlite3_changes(impl_->db);
  }

  Result<long long> insert(const std::string& sql) {
    auto result = execute(sql);
    if (!result) return std::unexpected(result.error());
    return sqlite3_last_insert_rowid(impl_->db);
  }
};

} // namespace jsonstore

#endif // JSONSTORE_DATABASE_HPP
