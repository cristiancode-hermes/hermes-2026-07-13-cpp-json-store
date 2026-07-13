# Architecture

## Data Flow

```
CLI input → Cli::parse() → CliOptions
    ↓
Database::put/get/remove/list/search/count/import/export
    ↓
SQLite3 (WAL mode)
    ↓
JSON output (stdout)
```

## Module Layout

```
include/jsonstore/
├── database.hpp    — RAII Database wrapper over sqlite3
│   └── class Database
│       ├── put(key, value)     — INSERT OR REPLACE
│       ├── get(key)            — SELECT by key
│       ├── remove(key)         — DELETE by key
│       ├── list(prefix)        — SELECT with optional LIKE
│       ├── search(term)         — Full-text search
│       ├── count()             — SELECT COUNT(*)
│       ├── import_json(json)   — Bulk insert from JSON array
│       └── export_all()        — Dump all to JSON array
│
└── cli.hpp         — CLI argument parser and command dispatcher
    └── class Cli
        ├── parse()              → CliOptions
        └── run(opts)            → exit code
```

## Error Handling

- `std::expected<T, Error>` for SQLite operations
- Error struct: { int code, std::string message }
- HTTP-style codes: 400 (bad input), 404 (not found), 500 (DB error)

## Dependencies

- **nlohmann_json** (header-only, FetchContent)
- **spdlog** (header-only, FetchContent)
- **SQLite3** (system package, pkg-config)
- **Catch2** (FetchContent, tests only)

## Security

- Basic SQL injection escape (single quote doubling)
- For production: use prepared statements with sqlite3_bind_*
