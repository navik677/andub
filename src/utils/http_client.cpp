#include "http_client.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <array>
#include <memory>
#include <cstdio>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>

namespace {
size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

size_t file_write_callback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}
} // namespace
#endif

namespace anime::http {

Response Client::get(const std::string& url, const std::map<std::string, std::string>& headers, int timeout_sec) {
#ifdef HAVE_LIBCURL
    Response resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error = "Failed to initialize CURL";
        return resp;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_sec));
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/124.0");

    struct curl_slist* chunk = nullptr;
    for (const auto& [k, v] : headers) {
        std::string h = k + ": " + v;
        chunk = curl_slist_append(chunk, h.c_str());
    }
    if (chunk) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp.error = curl_easy_strerror(res);
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        resp.status_code = static_cast<int>(http_code);
    }

    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    return resp;
#else
    // Fallback using system curl CLI
    Response resp;
    std::ostringstream cmd;
    cmd << "curl -s -L --max-time " << timeout_sec << " -A \"Mozilla/5.0 (X11; Linux x86_64)\" ";
    for (const auto& [k, v] : headers) {
        cmd << "-H \"" << k << ": " << v << "\" ";
    }
    cmd << "-w \"\\n%{http_code}\" \"" << url << "\"";

    std::array<char, 4096> buffer;
    std::string output;
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        resp.error = "Failed to run curl";
        return resp;
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }
    pclose(pipe);

    size_t last_newline = output.find_last_of('\n');
    if (last_newline != std::string::npos) {
        std::string code_str = output.substr(last_newline + 1);
        try {
            resp.status_code = std::stoi(code_str);
            resp.body = output.substr(0, last_newline);
        } catch (...) {
            resp.body = output;
            resp.status_code = 200;
        }
    } else {
        resp.body = output;
        resp.status_code = 200;
    }
    return resp;
#endif
}

Response Client::post(const std::string& url, const std::string& data, const std::map<std::string, std::string>& headers, int timeout_sec) {
#ifdef HAVE_LIBCURL
    Response resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error = "Failed to initialize CURL";
        return resp;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_sec));
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/124.0");

    struct curl_slist* chunk = nullptr;
    for (const auto& [k, v] : headers) {
        std::string h = k + ": " + v;
        chunk = curl_slist_append(chunk, h.c_str());
    }
    if (chunk) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp.error = curl_easy_strerror(res);
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        resp.status_code = static_cast<int>(http_code);
    }

    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    return resp;
#else
    Response resp;
    std::ostringstream cmd;
    cmd << "curl -s -L --max-time " << timeout_sec << " -A \"Mozilla/5.0 (X11; Linux x86_64)\" ";
    for (const auto& [k, v] : headers) {
        cmd << "-H \"" << k << ": " << v << "\" ";
    }
    cmd << "-d \"" << data << "\" -w \"\\n%{http_code}\" \"" << url << "\"";

    std::array<char, 4096> buffer;
    std::string output;
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        resp.error = "Failed to run curl";
        return resp;
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }
    pclose(pipe);

    size_t last_newline = output.find_last_of('\n');
    if (last_newline != std::string::npos) {
        std::string code_str = output.substr(last_newline + 1);
        try {
            resp.status_code = std::stoi(code_str);
            resp.body = output.substr(0, last_newline);
        } catch (...) {
            resp.body = output;
            resp.status_code = 200;
        }
    } else {
        resp.body = output;
        resp.status_code = 200;
    }
    return resp;
#endif
}

std::future<Response> Client::get_async(std::string url, std::map<std::string, std::string> headers, int timeout_sec) {
    return std::async(std::launch::async, [url = std::move(url), headers = std::move(headers), timeout_sec]() {
        return get(url, headers, timeout_sec);
    });
}

std::future<Response> Client::post_async(std::string url, std::string data, std::map<std::string, std::string> headers, int timeout_sec) {
    return std::async(std::launch::async, [url = std::move(url), data = std::move(data), headers = std::move(headers), timeout_sec]() {
        return post(url, data, headers, timeout_sec);
    });
}

bool Client::download_file(const std::string& url, const std::string& dest_path, int timeout_sec) {
#ifdef HAVE_LIBCURL
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    FILE* fp = fopen(dest_path.c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_sec));
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/124.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK);
#else
    std::ostringstream cmd;
    cmd << "curl -s -L --max-time " << timeout_sec << " -A \"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36\" -o \"" << dest_path << "\" \"" << url << "\"";
    return (std::system(cmd.str().c_str()) == 0);
#endif
}

} // namespace anime::http
