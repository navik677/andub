#pragma once

#include <string>
#include <map>
#include <functional>
#include <future>

namespace anime::http {

struct Response {
    int status_code = 0;
    std::string body;
    std::string error;
    bool success() const { return status_code >= 200 && status_code < 300 && error.empty(); }
};

// WinHTTP-based. Redirects are followed, gzip/deflate/brotli are decoded.
// cookie_jar: empty = no cookies are kept (each request is independent);
// any other name = an in-memory cookie session shared by all requests with that
// name for the lifetime of the process (e.g. "anitube" for the DLE PHPSESSID).
class Client {
public:
    static Response get(const std::string& url, const std::map<std::string, std::string>& headers = {}, int timeout_sec = 15, const std::string& cookie_jar = "");
    static Response post(const std::string& url, const std::string& data, const std::map<std::string, std::string>& headers = {}, int timeout_sec = 15, const std::string& cookie_jar = "");

    // Asynchronous versions returning future
    static std::future<Response> get_async(std::string url, std::map<std::string, std::string> headers = {}, int timeout_sec = 15, std::string cookie_jar = "");
    static std::future<Response> post_async(std::string url, std::string data, std::map<std::string, std::string> headers = {}, int timeout_sec = 15, std::string cookie_jar = "");

    // Download to file
    static bool download_file(const std::string& url, const std::string& dest_path, int timeout_sec = 20);
};

} // namespace anime::http
