#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "resmon/collectors/cpu_collector.hpp"
#include "resmon/collectors/gpu_collector.hpp"
#include "resmon/collectors/llama_collector.hpp"
#include "resmon/collectors/mem_collector.hpp"
#include "resmon/config.hpp"
#include "resmon/constants.hpp"
#include "resmon/sampler.hpp"
#include "support/capture_sink.hpp"
#include "support/fake_command_runner.hpp"
#include "support/fake_http_client.hpp"

namespace resmon {
namespace {

using testsupport::CaptureSink;
using testsupport::FakeCommandRunner;
using testsupport::FakeHttpClient;

std::string fixtureRoot() { return RESMON_TEST_FIXTURES_DIR; }

// Builds the same collector set main.cpp wires against real system
// resources, but pointed at fixtures/fakes -- exercises the whole
// collect -> merge -> publish pipeline with no real hardware, llama-server,
// or MQTT broker involved.
std::vector<std::unique_ptr<ICollector>> buildFixtureCollectors(std::shared_ptr<FakeHttpClient> http,
                                                                  std::shared_ptr<FakeCommandRunner> runner,
                                                                  const std::string& proc_root) {
  std::vector<std::unique_ptr<ICollector>> collectors;
  collectors.push_back(std::make_unique<CpuCollector>(fixtureRoot(), proc_root));
  collectors.push_back(std::make_unique<MemCollector>(proc_root));
  collectors.push_back(std::make_unique<GpuCollector>(runner));
  auto gpu_client = std::make_shared<NvidiaSmiClient>(runner);
  collectors.push_back(
      std::make_unique<LlamaCollector>(http, "http://fake-llama:11434", proc_root, gpu_client));
  return collectors;
}

std::shared_ptr<FakeCommandRunner> makeNvidiaSmiFake() {
  return std::make_shared<FakeCommandRunner>([](const std::vector<std::string>& argv) {
    CommandResult r;
    r.ok = true;
    if (argv.size() > 1 && argv[1].rfind("--query-gpu", 0) == 0) {
      r.stdout_text = "0, Fake GPU, 44, 48.6, 350, 33, 22, 22887, 24576, 510, 0\n";
    } else {
      r.stdout_text = "424242, 100\n";  // pid from tests/fixtures/proc/424242
    }
    return r;
  });
}

TEST(EndToEnd, HealthySampleProducesFullSchemaAndPublishesRetainedState) {
  auto http = std::make_shared<FakeHttpClient>();
  std::string base = "http://fake-llama:11434";

  HttpResponse health;
  health.ok = true;
  health.body = R"({"status":"ok"})";
  http->setResponse(base + std::string(constants::kLlamaHealthPath), health);

  HttpResponse slots;
  slots.ok = true;
  slots.body = "[]";
  http->setResponse(base + std::string(constants::kLlamaSlotsPath), slots);

  HttpResponse metrics;
  metrics.ok = true;
  metrics.body = "llamacpp:prompt_tokens_total 5\n";
  http->setResponse(base + std::string(constants::kLlamaMetricsPath), metrics);

  auto runner = makeNvidiaSmiFake();
  auto collectors = buildFixtureCollectors(http, runner, fixtureRoot());
  Sampler sampler(std::move(collectors), "test-host", constants::kSchemaVersion);

  Config cfg;
  cfg.topic_prefix = "resmon";
  cfg.hostname = "test-host";

  CaptureSink sink;
  nlohmann::json sample = sampler.sampleOnce();
  sink.publish(cfg.stateTopic(), sample.dump(), true);

  ASSERT_EQ(sink.captured().size(), 1u);
  EXPECT_EQ(sink.captured()[0].topic, "resmon/test-host/state");
  EXPECT_TRUE(sink.captured()[0].retain);

  nlohmann::json published = nlohmann::json::parse(sink.captured()[0].payload);
  EXPECT_EQ(published["schema"].get<int>(), constants::kSchemaVersion);
  EXPECT_EQ(published["host"].get<std::string>(), "test-host");
  EXPECT_TRUE(published.contains("ts"));
  EXPECT_TRUE(published.contains("uptime_s"));
  EXPECT_TRUE(published["llama"]["up"].get<bool>());
  EXPECT_TRUE(published.contains("cpu"));
  EXPECT_TRUE(published.contains("mem"));
  ASSERT_TRUE(published["gpu"].is_array());
  EXPECT_EQ(published["gpu"].size(), 1u);
  EXPECT_EQ(published["llama"]["proc"]["pid"].get<long>(), 424242);
}

TEST(EndToEnd, LlamaDownStillPublishesRestOfSample) {
  auto http = std::make_shared<FakeHttpClient>();  // no responses registered -> llama unreachable
  auto runner = std::make_shared<FakeCommandRunner>(
      [](const std::vector<std::string>&) { return CommandResult{}; });  // ok=false
  auto collectors = buildFixtureCollectors(http, runner, fixtureRoot());
  Sampler sampler(std::move(collectors), "test-host", constants::kSchemaVersion);

  nlohmann::json sample = sampler.sampleOnce();
  EXPECT_FALSE(sample["llama"]["up"].get<bool>());
  EXPECT_TRUE(sample.contains("mem"));  // other collectors unaffected by llama being down
  EXPECT_TRUE(sample.contains("cpu"));
  ASSERT_TRUE(sample["gpu"].is_array());  // gpu collector also unaffected
}

TEST(EndToEnd, AvailabilityStatusPublishedOnlineThenOfflineRetained) {
  // Mirrors what main.cpp does around the sampling loop: publish retained
  // "online" right after connecting, and retained "offline" on graceful
  // shutdown -- verifies the exact topic/payload/retain pattern a
  // subscriber (or the broker delivering the LWT on an unclean disconnect)
  // would see.
  Config cfg;
  cfg.topic_prefix = "resmon";
  cfg.hostname = "test-host";

  CaptureSink sink;
  sink.publish(cfg.statusTopic(), std::string(constants::kStatusOnline), true);
  sink.publish(cfg.statusTopic(), std::string(constants::kStatusOffline), true);

  ASSERT_EQ(sink.captured().size(), 2u);
  EXPECT_EQ(sink.captured()[0].topic, "resmon/test-host/status");
  EXPECT_EQ(sink.captured()[1].topic, "resmon/test-host/status");
  EXPECT_EQ(sink.captured()[0].payload, "online");
  EXPECT_EQ(sink.captured()[1].payload, "offline");
  EXPECT_TRUE(sink.captured()[0].retain);
  EXPECT_TRUE(sink.captured()[1].retain);
}

}  // namespace
}  // namespace resmon
