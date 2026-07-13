#include <jsonstore/database.hpp>
#include <jsonstore/cli.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdlib>

int main(int argc, char** argv) {
  // Setup logging
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_pattern("[%^%l%$] %v");

  jsonstore::Cli cli(argc, argv);
  auto opts = cli.parse();

  // Disable spdlog for non-verbose mode
  if (!opts.verbose) {
    spdlog::set_level(spdlog::level::warn);
  }

  return cli.run(opts);
}
