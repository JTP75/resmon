#pragma once

#include <string>

namespace resmon {

struct HttpResponse {
  bool ok = false;
  long status_code = 0;
  std::string body;
  std::string error;  // populated when ok == false
};

// Seam over HTTP GET so collectors are testable without a real server.
class IHttpClient {
 public:
  virtual ~IHttpClient() = default;
  virtual HttpResponse get(const std::string& url) const = 0;
};

class CurlHttpClient : public IHttpClient {
 public:
  explicit CurlHttpClient(int timeout_ms);
  HttpResponse get(const std::string& url) const override;

 private:
  int timeout_ms_;
};

}  // namespace resmon
