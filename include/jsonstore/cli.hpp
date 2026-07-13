#ifndef JSONSTORE_CLI_HPP
#define JSONSTORE_CLI_HPP

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <format>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "database.hpp"

using json = nlohmann::json;

namespace jsonstore {

struct CliOptions {
  std::string command;
  std::string db_path = "jsonstore.db";
  std::string key;
  std::string value;
  std::string prefix;
  std::string search_term;
  std::string file_path;
  bool help = false;
  bool version = false;
  bool verbose = false;
};

class Cli {
public:
  Cli(int argc, char** argv) : argc_(argc), argv_(argv) {}

  CliOptions parse() {
    CliOptions opts;

    for (int i = 1; i < argc_; ++i) {
      std::string_view arg{argv_[i]};

      if (arg == "--help" || arg == "-h") {
        opts.help = true;
        return opts;
      }
      if (arg == "--version" || arg == "-v") {
        opts.version = true;
        return opts;
      }
      if (arg == "--verbose") {
        opts.verbose = true;
        continue;
      }
      if (arg == "--db" && i + 1 < argc_) {
        opts.db_path = argv_[++i];
        continue;
      }
      if (arg == "--key" && i + 1 < argc_) {
        opts.key = argv_[++i];
        continue;
      }
      if (arg == "--value" && i + 1 < argc_) {
        opts.value = argv_[++i];
        continue;
      }
      if (arg == "--prefix" && i + 1 < argc_) {
        opts.prefix = argv_[++i];
        continue;
      }
      if (arg == "--search" && i + 1 < argc_) {
        opts.search_term = argv_[++i];
        continue;
      }
      if (arg == "--file" && i + 1 < argc_) {
        opts.file_path = argv_[++i];
        continue;
      }

      // First non-flag argument is the command
      if (opts.command.empty() && arg[0] != '-') {
        opts.command = arg;
      }
    }

    return opts;
  }

  int run(const CliOptions& opts) {
    if (opts.help) { print_help(); return 0; }
    if (opts.version) { print_version(); return 0; }

    if (opts.command.empty()) {
      std::println(std::cerr, "Error: No command specified. Use --help for usage.");
      return 1;
    }

    try {
      Database db(opts.db_path);

      // Make a mutable copy for import which needs to modify file_path
      CliOptions mutable_opts = opts;

      if (opts.command == "put") {
        return cmd_put(db, opts);
      } else if (opts.command == "get") {
        return cmd_get(db, opts);
      } else if (opts.command == "delete" || opts.command == "del") {
        return cmd_delete(db, opts);
      } else if (opts.command == "list") {
        return cmd_list(db, opts);
      } else if (opts.command == "search") {
        return cmd_search(db, opts);
      } else if (opts.command == "count") {
        return cmd_count(db, opts);
      } else if (opts.command == "import") {
        return cmd_import(db, mutable_opts);
      } else if (opts.command == "export") {
        return cmd_export(db, opts);
      } else {
        std::println(std::cerr, "Error: Unknown command '{}'. Use --help.", opts.command);
        return 1;
      }
    } catch (const std::exception& e) {
      std::println(std::cerr, "Error: {}", e.what());
      return 1;
    }
  }

  static void print_help() {
    std::println(std::cout, "jsonstore v{} -- JSON document store with SQLite backend", "1.0.0");
    std::println(std::cout, "");
    std::println(std::cout, "Usage: jsonstore <command> [options]");
    std::println(std::cout, "");
    std::println(std::cout, "Commands:");
    std::println(std::cout, "  put    --key <k> --value <v>    Store a JSON document");
    std::println(std::cout, "  get    --key <k>                Retrieve a document by key");
    std::println(std::cout, "  delete --key <k>                Delete a document by key");
    std::println(std::cout, "  list   [--prefix <p>]           List all documents");
    std::println(std::cout, "  search --search <term>          Search documents");
    std::println(std::cout, "  count                           Count documents");
    std::println(std::cout, "  import --file <path>            Import JSON array from file");
    std::println(std::cout, "  export                          Export all documents as JSON");
    std::println(std::cout, "");
    std::println(std::cout, "Options:");
    std::println(std::cout, "  --db <path>      Database file path (default: jsonstore.db)");
    std::println(std::cout, "  --key <k>        Document key");
    std::println(std::cout, "  --value <v>      JSON value (stringified)");
    std::println(std::cout, "  --prefix <p>     Key prefix filter");
    std::println(std::cout, "  --search <term>  Search term");
    std::println(std::cout, "  --file <path>    Input file path");
    std::println(std::cout, "  --verbose        Enable verbose logging");
    std::println(std::cout, "  --help, -h       Show this help");
    std::println(std::cout, "  --version, -v    Show version");
  }

  static void print_version() {
    std::println(std::cout, "jsonstore version 1.0.0");
    std::println(std::cout, "C++23, SQLite3, nlohmann_json, spdlog");
    std::println(std::cout, "https://github.com/cristiancode-hermes/hermes-2026-07-13-cpp-json-store");
  }

private:
  int argc_;
  char** argv_;

  int cmd_put(Database& db, const CliOptions& opts) {
    if (opts.key.empty() || opts.value.empty()) {
      std::println(std::cerr, "Error: put requires --key and --value");
      return 1;
    }
    json value;
    try {
      value = json::parse(opts.value);
    } catch (json::parse_error&) {
      // If not valid JSON, store as string
      value = opts.value;
    }
    auto result = db.put(opts.key, value);
    if (!result) {
      std::println(std::cerr, "Error: {} (code {})", result.error().message, result.error().code);
      return 1;
    }
    std::println(std::cout, "Stored document '{}' (id: {})", opts.key, *result);
    return 0;
  }

  int cmd_get(Database& db, const CliOptions& opts) {
    if (opts.key.empty()) {
      std::println(std::cerr, "Error: get requires --key");
      return 1;
    }
    auto result = db.get(opts.key);
    if (!result) {
      if (result.error().code == 404) {
        std::println(std::cerr, "Not found: '{}'", opts.key);
        return 1;
      }
      std::println(std::cerr, "Error: {} (code {})", result.error().message, result.error().code);
      return 1;
    }
    std::println(std::cout, "{}", (*result).dump(2));
    return 0;
  }

  int cmd_delete(Database& db, const CliOptions& opts) {
    if (opts.key.empty()) {
      std::println(std::cerr, "Error: delete requires --key");
      return 1;
    }
    auto result = db.remove(opts.key);
    if (!result) {
      std::println(std::cerr, "Error: {} (code {})", result.error().message, result.error().code);
      return 1;
    }
    std::println(std::cout, "Deleted {} row(s) for key '{}'", *result, opts.key);
    return 0;
  }

  int cmd_list(Database& db, const CliOptions& opts) {
    auto result = db.list(opts.prefix);
    if (!result) {
      std::println(std::cerr, "Error: {} (code {})", result.error().message, result.error().code);
      return 1;
    }
    json output = json::array();
    for (auto& row : *result) {
      output.push_back(row);
    }
    std::println(std::cout, "{}", output.dump(2));
    return 0;
  }

  int cmd_search(Database& db, const CliOptions& opts) {
    if (opts.search_term.empty()) {
      std::println(std::cerr, "Error: search requires --search");
      return 1;
    }
    auto result = db.search(opts.search_term);
    if (!result) {
      std::println(std::cerr, "Error: {} (code {})", result.error().message, result.error().code);
      return 1;
    }
    json output = json::array();
    for (auto& row : *result) {
      output.push_back(row);
    }
    std::println(std::cout, "{}", output.dump(2));
    return 0;
  }

  int cmd_count(Database& db, const CliOptions&) {
    auto result = db.count();
    if (!result) {
      std::println(std::cerr, "Error: {} (code {})", result.error().message, result.error().code);
      return 1;
    }
    std::println(std::cout, "{}", *result);
    return 0;
  }

  int cmd_import(Database& db, CliOptions& opts) {
    if (opts.file_path.empty()) {
      // Read from stdin
      std::stringstream buffer;
      buffer << std::cin.rdbuf();
      if (buffer.str().empty()) {
        std::println(std::cerr, "Error: import requires --file or stdin input");
        return 1;
      }
      opts.value = buffer.str();
    } else {
      std::ifstream file(opts.file_path);
      if (!file) {
        std::println(std::cerr, "Error: Cannot open file '{}'", opts.file_path);
        return 1;
      }
      std::stringstream buffer;
      buffer << file.rdbuf();
      opts.value = buffer.str();
    }

    json docs;
    try {
      docs = json::parse(opts.value);
    } catch (json::parse_error& e) {
      std::println(std::cerr, "Error: Invalid JSON: {}", e.what());
      return 1;
    }

    auto count = db.import_json(docs);
    if (!count) {
      std::println(std::cerr, "Error: {} (code {})", count.error().message, count.error().code);
      return 1;
    }
    std::println(std::cout, "Imported {} document(s)", *count);
    return 0;
  }

  int cmd_export(Database& db, const CliOptions&) {
    auto result = db.export_all();
    if (!result) {
      std::println(std::cerr, "Error: {} (code {})", result.error().message, result.error().code);
      return 1;
    }
    std::println(std::cout, "{}", result->dump(2));
    return 0;
  }
};

} // namespace jsonstore

#endif // JSONSTORE_CLI_HPP
