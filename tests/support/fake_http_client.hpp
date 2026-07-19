#pragma once

#include <map>
#include <string>

#include "resmon/http_client.hpp"

namespace resmon::testsupport {

// Maps exact URLs to canned responses. Unregistered URLs return a failed
// response rather than throwing, matching CurlHttpClient's contract of
// never throwing for network-shaped failures.
class FakeHttpClient : public IHttpClient {
 public:
  void setResponse(const std::string& url, HttpResponse response) { responses_[url] = std::move(response); }

  HttpResponse get(const std::string& url) const override {
    auto it = responses_.find(url);
    if (it == responses_.end()) {
      HttpResponse r;
      r.ok = false;
      r.error = "no fixture registered for URL: " + url;
      return r;
    }
    return it->second;
  }

 private:
  std::map<std::string, HttpResponse> responses_;
};

}  // namespace resmon::testsupport
