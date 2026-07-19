#pragma once

#include <string>
#include <vector>

#include "resmon/mqtt_publisher.hpp"

namespace resmon::testsupport {

struct CapturedPublish {
  std::string topic;
  std::string payload;
  bool retain;
};

// Records every publish() call instead of talking to a broker, so the
// sampler -> publish pipeline is verifiable without any network I/O.
class CaptureSink : public IMqttSink {
 public:
  void publish(const std::string& topic, const std::string& payload, bool retain) override {
    captured_.push_back({topic, payload, retain});
  }

  const std::vector<CapturedPublish>& captured() const { return captured_; }

 private:
  std::vector<CapturedPublish> captured_;
};

}  // namespace resmon::testsupport
