#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <jsonstore/database.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

using json = nlohmann::json;
namespace fs = std::filesystem;

const std::string TEST_DB = "test_jsonstore.db";

struct DbFixture {
  jsonstore::Database db;
  DbFixture() : db(TEST_DB) {}
  ~DbFixture() {
    db.~Database();
    fs::remove(TEST_DB);
    fs::remove(TEST_DB + "-wal");
    fs::remove(TEST_DB + "-shm");
  }
};

// ── Unit Tests ──

TEST_CASE("Database opens and schema is created", "[database]") {
  // Clean up before test
  fs::remove(TEST_DB);
  fs::remove(TEST_DB + "-wal");
  fs::remove(TEST_DB + "-shm");

  {
    jsonstore::Database db(TEST_DB);
    auto count = db.count();
    REQUIRE(count.has_value());
    REQUIRE(*count == 0);
  }

  fs::remove(TEST_DB);
  fs::remove(TEST_DB + "-wal");
  fs::remove(TEST_DB + "-shm");
}

TEST_CASE("Put and Get a document", "[database]") {
  fs::remove(TEST_DB);
  {
    jsonstore::Database db(TEST_DB);
    json doc = {{"name", "test"}, {"value", 42}, {"active", true}};

    auto put_result = db.put("test-key", doc);
    REQUIRE(put_result.has_value());

    auto get_result = db.get("test-key");
    REQUIRE(get_result.has_value());

    auto value = (*get_result)["value"];
    REQUIRE(value.is_string());
    json parsed = json::parse(value.get<std::string>());
    REQUIRE(parsed["name"] == "test");
    REQUIRE(parsed["value"] == 42);
    REQUIRE(parsed["active"] == true);
  }
  fs::remove(TEST_DB);
}

TEST_CASE("Get non-existent key returns error", "[database]") {
  fs::remove(TEST_DB);
  {
    jsonstore::Database db(TEST_DB);
    auto result = db.get("non-existent");
    REQUIRE(!result.has_value());
    REQUIRE(result.error().code == 404);
  }
  fs::remove(TEST_DB);
}

TEST_CASE("Delete a document", "[database]") {
  fs::remove(TEST_DB);
  {
    jsonstore::Database db(TEST_DB);
    db.put("delete-me", json{{"data", "to-delete"}});
    auto del_result = db.remove("delete-me");
    REQUIRE(del_result.has_value());
    REQUIRE(*del_result > 0);

    auto get_result = db.get("delete-me");
    REQUIRE(!get_result.has_value());
  }
  fs::remove(TEST_DB);
}

TEST_CASE("List documents with prefix filter", "[database]") {
  fs::remove(TEST_DB);
  {
    jsonstore::Database db(TEST_DB);
    db.put("user:alice", json{{"role", "admin"}});
    db.put("user:bob", json{{"role", "user"}});
    db.put("config:app", json{{"debug", true}});

    auto all = db.list("");
    REQUIRE(all.has_value());
    REQUIRE(all->size() == 3);

    auto users = db.list("user:");
    REQUIRE(users.has_value());
    REQUIRE(users->size() == 2);

    auto configs = db.list("config:");
    REQUIRE(configs.has_value());
    REQUIRE(configs->size() == 1);
  }
  fs::remove(TEST_DB);
}

TEST_CASE("Search documents", "[database]") {
  fs::remove(TEST_DB);
  {
    jsonstore::Database db(TEST_DB);
    db.put("doc1", json{{"title", "C++ Guide"}, {"author", "Bjarne"}});
    db.put("doc2", json{{"title", "Python Guide"}, {"author", "Guido"}});

    auto results = db.search("Guide");
    REQUIRE(results.has_value());
    REQUIRE(results->size() == 2);

    auto python_results = db.search("Python");
    REQUIRE(python_results.has_value());
    REQUIRE(python_results->size() == 1);
  }
  fs::remove(TEST_DB);
}

TEST_CASE("Count documents", "[database]") {
  fs::remove(TEST_DB);
  {
    jsonstore::Database db(TEST_DB);
    REQUIRE(*db.count() == 0);
    db.put("a", json{{"v", 1}});
    REQUIRE(*db.count() == 1);
    db.put("b", json{{"v", 2}});
    db.put("c", json{{"v", 3}});
    REQUIRE(*db.count() == 3);
  }
  fs::remove(TEST_DB);
}

TEST_CASE("Import JSON array", "[database]") {
  fs::remove(TEST_DB);
  {
    jsonstore::Database db(TEST_DB);
    json docs = json::array();
    docs.push_back({{"key", "imp1"}, {"value", {{"data", "first"}}}});
    docs.push_back({{"key", "imp2"}, {"value", {{"data", "second"}}}});

    auto count = db.import_json(docs);
    REQUIRE(count.has_value());
    REQUIRE(*count == 2);
    REQUIRE(*db.count() == 2);
  }
  fs::remove(TEST_DB);
}

TEST_CASE("Export all documents", "[database]") {
  fs::remove(TEST_DB);
  {
    jsonstore::Database db(TEST_DB);
    db.put("ex1", json{{"val", 1}});
    db.put("ex2", json{{"val", 2}});

    auto exported = db.export_all();
    REQUIRE(exported.has_value());
    REQUIRE(exported->is_array());
    REQUIRE(exported->size() == 2);
  }
  fs::remove(TEST_DB);
}

TEST_CASE("Put replaces existing key", "[database]") {
  fs::remove(TEST_DB);
  {
    jsonstore::Database db(TEST_DB);
    db.put("same-key", json{{"version", 1}});
    db.put("same-key", json{{"version", 2}});

    auto result = db.get("same-key");
    REQUIRE(result.has_value());

    auto value = (*result)["value"];
    REQUIRE(value.is_string());
    json parsed = json::parse(value.get<std::string>());
    REQUIRE(parsed["version"] == 2);
    REQUIRE(*db.count() == 1);
  }
  fs::remove(TEST_DB);
}

// ── Benchmark ──

TEST_CASE("JSON roundtrip performance", "[benchmark]") {
  json obj = {{"name", "benchmark-test"}, {"count", 1000}, {"tags", json::array({"a", "b", "c"})},
              {"metadata", {{"created", "2026-07-13"}, {"version", 1}}}};

  BENCHMARK("Serialize + deserialize JSON") {
    auto serialized = obj.dump();
    return json::parse(serialized);
  };
}

TEST_CASE("Database put performance", "[database][benchmark]") {
  fs::remove(TEST_DB);
  jsonstore::Database db(TEST_DB);

  BENCHMARK("Put 100 documents") {
    for (int i = 0; i < 100; ++i) {
      auto key = std::format("bench:{}", i);
      db.put(key, json{{"index", i}, {"data", "benchmark data for performance testing"}});
    }
    return 100;
  };

  fs::remove(TEST_DB);
}
