#include "resmon/collectors/llama_collector.hpp"

#include "resmon/constants.hpp"
#include "resmon/proc_locator.hpp"
#include "resmon/prometheus.hpp"

namespace resmon {

namespace c = constants;

LlamaCollector::LlamaCollector(std::shared_ptr<IHttpClient> http, std::string base_url,
                                std::string proc_root, std::shared_ptr<NvidiaSmiClient> gpu_client)
    : http_(std::move(http)),
      base_url_(std::move(base_url)),
      proc_root_(std::move(proc_root)),
      gpu_client_(std::move(gpu_client)) {}

std::string LlamaCollector::name() const { return "llama"; }

nlohmann::json LlamaCollector::collect() {
  nlohmann::json j;

  auto health_resp = http_->get(base_url_ + std::string(c::kLlamaHealthPath));
  j["up"] = health_resp.ok;

  if (health_resp.ok) {
    try {
      j["health"] = nlohmann::json::parse(health_resp.body);
    } catch (const nlohmann::json::exception&) {
      j["health"] = nullptr;
    }
  } else {
    j["health"] = nullptr;
  }

  auto metrics_resp = http_->get(base_url_ + std::string(c::kLlamaMetricsPath));
  nlohmann::json throughput, totals;
  if (metrics_resp.ok) {
    auto m = parsePrometheusText(metrics_resp.body);
    throughput["prompt_tps"] = promGet(m, "llamacpp:prompt_tokens_seconds");
    throughput["predicted_tps"] = promGet(m, "llamacpp:predicted_tokens_seconds");
    throughput["requests_processing"] = promGet(m, "llamacpp:requests_processing");
    throughput["requests_deferred"] = promGet(m, "llamacpp:requests_deferred");
    throughput["busy_slots_per_decode"] = promGet(m, "llamacpp:n_busy_slots_per_decode");

    totals["prompt_tokens"] = promGet(m, "llamacpp:prompt_tokens_total");
    totals["tokens_predicted"] = promGet(m, "llamacpp:tokens_predicted_total");
    totals["n_decode"] = promGet(m, "llamacpp:n_decode_total");
    totals["n_tokens_max"] = promGet(m, "llamacpp:n_tokens_max");
  } else {
    throughput = nullptr;
    totals = nullptr;
  }
  j["throughput"] = throughput;
  j["totals"] = totals;

  auto slots_resp = http_->get(base_url_ + std::string(c::kLlamaSlotsPath));
  if (slots_resp.ok) {
    try {
      j["slots"] = nlohmann::json::parse(slots_resp.body);
    } catch (const nlohmann::json::exception&) {
      j["slots"] = nlohmann::json::array();
    }
  } else {
    j["slots"] = nlohmann::json::array();
  }

  auto proc = findLlamaProcess(proc_root_, std::string(c::kLlamaBinaryNameHint));
  nlohmann::json proc_json;
  if (proc) {
    proc_json["pid"] = proc->pid;
    proc_json["rss_bytes"] = proc->rss_bytes;
    auto vram = gpu_client_ ? gpu_client_->vramUsedBytesForPid(proc->pid) : std::nullopt;
    proc_json["vram_bytes"] = vram ? nlohmann::json(*vram) : nlohmann::json(nullptr);
  } else {
    proc_json["pid"] = nullptr;
    proc_json["rss_bytes"] = nullptr;
    proc_json["vram_bytes"] = nullptr;
  }
  j["proc"] = proc_json;

  return j;
}

}  // namespace resmon
