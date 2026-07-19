#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "resmon/collectors/gpu_collector.hpp"
#include "resmon/collectors/llama_collector.hpp"
#include "resmon/collectors/mem_collector.hpp"
#include "resmon/constants.hpp"
#include "support/fake_command_runner.hpp"
#include "support/fake_http_client.hpp"

namespace resmon {
namespace {

using testsupport::FakeCommandRunner;
using testsupport::FakeHttpClient;

std::string fixtureRoot() { return RESMON_TEST_FIXTURES_DIR; }

// --- MemCollector ---

TEST(MemCollectorSchema, ProducesExpectedFieldsFromFixture) {
  MemCollector collector(fixtureRoot());
  auto j = collector.collect();

  // From tests/fixtures/proc/meminfo: MemTotal 49269476 kB, MemAvailable 35279800 kB.
  EXPECT_EQ(j["total_bytes"].get<uint64_t>(), 49269476ULL * 1024);
  EXPECT_EQ(j["available_bytes"].get<uint64_t>(), 35279800ULL * 1024);
  EXPECT_EQ(j["used_bytes"].get<uint64_t>(), (49269476ULL - 35279800ULL) * 1024);
  EXPECT_EQ(j["swap_total_bytes"].get<uint64_t>(), 2097148ULL * 1024);
  EXPECT_EQ(j["swap_used_bytes"].get<uint64_t>(), (2097148ULL - 1606284ULL) * 1024);
}

TEST(MemCollectorSchema, MissingMeminfoYieldsAllNulls) {
  MemCollector collector(fixtureRoot() + "/nonexistent");
  auto j = collector.collect();
  EXPECT_TRUE(j["total_bytes"].is_null());
  EXPECT_TRUE(j["used_bytes"].is_null());
  EXPECT_TRUE(j["swap_total_bytes"].is_null());
}

// --- GpuCollector ---

TEST(GpuCollectorSchema, ParsesCannedNvidiaSmiOutputIntoArray) {
  auto runner = std::make_shared<FakeCommandRunner>([](const std::vector<std::string>& argv) {
    CommandResult r;
    r.ok = true;
    r.stdout_text =
        "0, NVIDIA GeForce RTX 3090, 44, 48.64, 350.00, 33, 22, 22887, 24576, 510, 0\n";
    return r;
  });
  GpuCollector collector(runner);
  auto j = collector.collect();

  ASSERT_TRUE(j.is_array());
  ASSERT_EQ(j.size(), 1u);
  EXPECT_EQ(j[0]["index"].get<int>(), 0);
  EXPECT_EQ(j[0]["name"].get<std::string>(), "NVIDIA GeForce RTX 3090");
  EXPECT_DOUBLE_EQ(j[0]["temp_c"].get<double>(), 44.0);
  EXPECT_DOUBLE_EQ(j[0]["power_w"].get<double>(), 48.64);
  EXPECT_EQ(j[0]["vram_used_bytes"].get<uint64_t>(), 22887ULL * 1024 * 1024);
  EXPECT_EQ(j[0]["vram_total_bytes"].get<uint64_t>(), 24576ULL * 1024 * 1024);
}

TEST(GpuCollectorSchema, NotAvailableFanBecomesNull) {
  auto runner = std::make_shared<FakeCommandRunner>([](const std::vector<std::string>&) {
    CommandResult r;
    r.ok = true;
    r.stdout_text = "0, Some GPU, 44, 48.64, 350.00, 33, 22, 22887, 24576, 510, [N/A]\n";
    return r;
  });
  GpuCollector collector(runner);
  auto j = collector.collect();
  ASSERT_EQ(j.size(), 1u);
  EXPECT_TRUE(j[0]["fan_pct"].is_null());
}

TEST(GpuCollectorSchema, CommandFailureYieldsEmptyArray) {
  auto runner = std::make_shared<FakeCommandRunner>([](const std::vector<std::string>&) {
    CommandResult r;
    r.ok = false;
    r.error = "nvidia-smi not found";
    return r;
  });
  GpuCollector collector(runner);
  auto j = collector.collect();
  EXPECT_TRUE(j.is_array());
  EXPECT_TRUE(j.empty());
}

// --- LlamaCollector ---

TEST(LlamaCollectorSchema, HealthyServerProducesFullSchema) {
  auto http = std::make_shared<FakeHttpClient>();
  std::string base = "http://fake-llama:11434";
  HttpResponse health;
  health.ok = true;
  health.status_code = 200;
  health.body = R"({"status":"ok"})";
  http->setResponse(base + std::string(constants::kLlamaHealthPath), health);

  HttpResponse slots;
  slots.ok = true;
  slots.body = R"([{"id":0,"is_processing":false,"n_ctx":200192}])";
  http->setResponse(base + std::string(constants::kLlamaSlotsPath), slots);

  HttpResponse metrics;
  metrics.ok = true;
  metrics.body =
      "llamacpp:prompt_tokens_total 42\n"
      "llamacpp:predicted_tokens_seconds 17.5\n"
      "llamacpp:requests_processing 1\n";
  http->setResponse(base + std::string(constants::kLlamaMetricsPath), metrics);

  auto runner = std::make_shared<FakeCommandRunner>([](const std::vector<std::string>& argv) {
    CommandResult r;
    r.ok = true;
    // Matches pid 424242 from tests/fixtures/proc/424242/{cmdline,status}.
    r.stdout_text = "424242, 22016\n";
    return r;
  });
  auto gpu_client = std::make_shared<NvidiaSmiClient>(runner);

  LlamaCollector collector(http, base, fixtureRoot(), gpu_client);
  auto j = collector.collect();

  EXPECT_TRUE(j["up"].get<bool>());
  EXPECT_EQ(j["health"]["status"].get<std::string>(), "ok");
  EXPECT_DOUBLE_EQ(j["throughput"]["predicted_tps"].get<double>(), 17.5);
  EXPECT_DOUBLE_EQ(j["totals"]["prompt_tokens"].get<double>(), 42.0);
  ASSERT_TRUE(j["slots"].is_array());
  EXPECT_EQ(j["slots"][0]["id"].get<int>(), 0);
  EXPECT_EQ(j["proc"]["pid"].get<long>(), 424242);
  EXPECT_EQ(j["proc"]["rss_bytes"].get<uint64_t>(), 8983080ULL * 1024);
  EXPECT_EQ(j["proc"]["vram_bytes"].get<uint64_t>(), 22016ULL * 1024 * 1024);
}

TEST(LlamaCollectorSchema, UnreachableServerYieldsUpFalseAndNulls) {
  auto http = std::make_shared<FakeHttpClient>();  // no responses registered -> all fail
  auto runner = std::make_shared<FakeCommandRunner>([](const std::vector<std::string>&) {
    return CommandResult{};  // ok=false by default
  });
  auto gpu_client = std::make_shared<NvidiaSmiClient>(runner);

  LlamaCollector collector(http, "http://fake-llama:11434", fixtureRoot() + "/nonexistent", gpu_client);
  auto j = collector.collect();

  EXPECT_FALSE(j["up"].get<bool>());
  EXPECT_TRUE(j["health"].is_null());
  EXPECT_TRUE(j["throughput"].is_null());
  EXPECT_TRUE(j["totals"].is_null());
  ASSERT_TRUE(j["slots"].is_array());
  EXPECT_TRUE(j["slots"].empty());
  EXPECT_TRUE(j["proc"]["pid"].is_null());
}

}  // namespace
}  // namespace resmon
