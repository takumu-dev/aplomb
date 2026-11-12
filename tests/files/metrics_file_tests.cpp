#include <aplomb/files/metrics_file.hpp>

#include <aplomb/files/file_error.hpp>
#include <aplomb/files/scenario_desc.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <string>

TEST_CASE("a metrics file gives the scenario, the step rate and each metric's value", "[files]") {
    const aplomb::RunMetrics metrics{
        .scenario = "drop",
        .step_rate = 120.0F,
        .values =
            {
                {.metric = aplomb::Metric::max_speed_after_3s, .value = 0.0042},
                {.metric = aplomb::Metric::nan_count, .value = 0.0},
            },
    };

    CHECK(aplomb::write_metrics_file(metrics) == R"({
  "format": "aplomb.metrics",
  "version": 1,
  "scenario": "drop",
  "step_rate": 120,
  "metrics": {
    "max_speed_after_3s": 0.0042,
    "nan_count": 0
  }
}
)");
}

TEST_CASE(
    "a metrics file escapes the scenario's name, giving a control character its short escape "
    "where it has one, writes each number in its shortest form, and a value that isn't finite "
    "as null",
    "[files]") {
    const aplomb::RunMetrics metrics{
        .scenario = "a \"drop\"\\\n\t\x01",
        .step_rate = 59.9F,
        .values =
            {
                {.metric = aplomb::Metric::nan_count, .value = 3.0},
                {
                    .metric = aplomb::Metric::max_speed_after_3s,
                    .value = std::numeric_limits<double>::infinity(),
                },
            },
    };

    CHECK(aplomb::write_metrics_file(metrics) == R"({
  "format": "aplomb.metrics",
  "version": 1,
  "scenario": "a \"drop\"\\\n\t\u0001",
  "step_rate": 59.9,
  "metrics": {
    "nan_count": 3,
    "max_speed_after_3s": null
  }
}
)");
}

TEST_CASE("a metrics file of no metrics has an empty object of them", "[files]") {
    const std::string text = aplomb::write_metrics_file({.scenario = "idle", .step_rate = 60.0F});

    CHECK(text.ends_with("  \"metrics\": {}\n}\n"));
}

TEST_CASE("a saved metrics file holds what is written, replacing what its path held", "[files]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "aplomb_saved_metrics_file.json";
    const aplomb::RunMetrics metrics{
        .scenario = "drop",
        .step_rate = 60.0F,
        .values = {{.metric = aplomb::Metric::nan_count, .value = 0.0}},
    };
    std::ofstream(path) << "an older, longer file, which the saved one must replace entirely";

    const auto saved = aplomb::save_metrics_file(path, metrics);

    REQUIRE(saved.has_value());
    std::ifstream stream(path, std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>()};
    CHECK(text == aplomb::write_metrics_file(metrics));
    stream.close();
    std::filesystem::remove(path);
}

TEST_CASE("a metrics file in a directory that doesn't exist can't be written, and the error names "
          "it",
          "[files]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "aplomb_no_such_directory" / "metrics.json";

    const auto saved = aplomb::save_metrics_file(path, {.scenario = "drop", .step_rate = 60.0F});

    REQUIRE_FALSE(saved.has_value());
    CHECK(saved.error().code == aplomb::FileErrorCode::unwritable);
    CHECK(saved.error().problems.empty());
    CHECK(
        std::format("{}", saved.error()) ==
        std::format("the metrics file \"{}\" can't be written: it can't be opened", path.string()));
}
