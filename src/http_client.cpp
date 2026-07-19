#include "resmon/http_client.hpp"

#include <curl/curl.h>

#include <mutex>

namespace resmon {

namespace {

void ensureCurlGlobalInit() {
  static std::once_flag flag;
  std::call_once(flag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* out = static_cast<std::string*>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

}  // namespace

CurlHttpClient::CurlHttpClient(int timeout_ms) : timeout_ms_(timeout_ms) {
  ensureCurlGlobalInit();
}

HttpResponse CurlHttpClient::get(const std::string& url) const {
  HttpResponse resp;

  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    resp.error = "curl_easy_init failed";
    return resp;
  }

  std::string body;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms_));
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    resp.error = curl_easy_strerror(res);
    curl_easy_cleanup(curl);
    return resp;
  }

  long status_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
  curl_easy_cleanup(curl);

  resp.status_code = status_code;
  resp.body = std::move(body);
  resp.ok = status_code >= 200 && status_code < 300;
  if (!resp.ok && resp.error.empty()) {
    resp.error = "HTTP status " + std::to_string(status_code);
  }
  return resp;
}

}  // namespace resmon
