#include "resmon/prometheus.hpp"

#include <gtest/gtest.h>

namespace resmon {
namespace {

// A trimmed sample matching real llama-server /metrics output (counters and
// gauges), used to verify parsing of the actual exposition format we consume.
constexpr const char* kSampleText = R"(# HELP llamacpp:prompt_tokens_total Number of prompt tokens processed.
# TYPE llamacpp:prompt_tokens_total counter
llamacpp:prompt_tokens_total 1234
# HELP llamacpp:predicted_tokens_seconds Average generation throughput in tokens/s.
# TYPE llamacpp:predicted_tokens_seconds gauge
llamacpp:predicted_tokens_seconds 17.5
# HELP llamacpp:requests_processing Number of requests processing.
# TYPE llamacpp:requests_processing gauge
llamacpp:requests_processing 0
)";

TEST(Prometheus, ParsesCountersAndGauges) {
  auto metrics = parsePrometheusText(kSampleText);
  EXPECT_EQ(metrics.size(), 3u);
  EXPECT_DOUBLE_EQ(promGet(metrics, "llamacpp:prompt_tokens_total"), 1234.0);
  EXPECT_DOUBLE_EQ(promGet(metrics, "llamacpp:predicted_tokens_seconds"), 17.5);
  EXPECT_DOUBLE_EQ(promGet(metrics, "llamacpp:requests_processing"), 0.0);
}

TEST(Prometheus, SkipsCommentsAndBlankLines) {
  auto metrics = parsePrometheusText("# just a comment\n\n\n# TYPE foo gauge\n");
  EXPECT_TRUE(metrics.empty());
}

TEST(Prometheus, StripsLabelsFromMetricName) {
  auto metrics = parsePrometheusText("http_requests_total{method=\"GET\",code=\"200\"} 42\n");
  EXPECT_DOUBLE_EQ(promGet(metrics, "http_requests_total"), 42.0);
}

TEST(Prometheus, SkipsMalformedLinesWithoutThrowing) {
  std::string text =
      "not_a_valid_line_at_all\n"
      "metric_with_bad_value not-a-number\n"
      "good_metric 5\n"
      "trailing_garbage 5abc\n";
  PrometheusMetrics metrics;
  EXPECT_NO_THROW(metrics = parsePrometheusText(text));
  EXPECT_DOUBLE_EQ(promGet(metrics, "good_metric"), 5.0);
  EXPECT_EQ(metrics.count("metric_with_bad_value"), 0u);
  EXPECT_EQ(metrics.count("trailing_garbage"), 0u);
}

TEST(Prometheus, MissingKeyReturnsDefault) {
  PrometheusMetrics metrics;
  EXPECT_DOUBLE_EQ(promGet(metrics, "absent"), 0.0);
  EXPECT_DOUBLE_EQ(promGet(metrics, "absent", -1.0), -1.0);
}

TEST(Prometheus, EmptyInputYieldsEmptyMap) {
  EXPECT_TRUE(parsePrometheusText("").empty());
}

TEST(Prometheus, NegativeAndFractionalValuesParse) {
  auto metrics = parsePrometheusText("neg_metric -3.5\npos_metric 0.001\n");
  EXPECT_DOUBLE_EQ(promGet(metrics, "neg_metric"), -3.5);
  EXPECT_DOUBLE_EQ(promGet(metrics, "pos_metric"), 0.001);
}

}  // namespace
}  // namespace resmon
