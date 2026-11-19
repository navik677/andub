// HTTP client on WinHTTP: no extra DLLs, TLS and certificates come from the
// system (Schannel + Windows certificate store), proxy settings follow Windows.
#include "http_client.hpp"

#include <windows.h>
#include <winhttp.h>

#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace {

constexpr wchar_t kUserAgent[] =
    L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36";

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::string win_error(DWORD code) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "WinHTTP error %lu", static_cast<unsigned long>(code));
    switch (code) {
    case ERROR_WINHTTP_TIMEOUT: return "Timeout was reached";
    case ERROR_WINHTTP_NAME_NOT_RESOLVED: return "Couldn't resolve host name";
    case ERROR_WINHTTP_CANNOT_CONNECT: return "Couldn't connect to server";
    case ERROR_WINHTTP_SECURE_FAILURE: return "SSL connect error";
    case ERROR_WINHTTP_CONNECTION_ERROR: return "Connection reset";
    case ERROR_WINHTTP_REDIRECT_FAILED: return "Redirect failed";
    default: return buf;
    }
}

struct Handle {
    HINTERNET h = nullptr;
    Handle() = default;
    explicit Handle(HINTERNET x) : h(x) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle() { if (h) WinHttpCloseHandle(h); }
    explicit operator bool() const { return h != nullptr; }
};

HINTERNET open_session(bool keep_cookies) {
    HINTERNET s = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) // WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY needs Windows 8.1+
        s = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return nullptr;
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    if (!WinHttpSetOption(s, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols)) {
        protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2; // TLS 1.3 needs Windows 11
        WinHttpSetOption(s, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols);
    }
    DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
    WinHttpSetOption(s, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof decompression);
    if (!keep_cookies) {
        // Requests without a cookie jar behave like plain curl: nothing is remembered
        DWORD flags = WINHTTP_DISABLE_COOKIES;
        WinHttpSetOption(s, WINHTTP_OPTION_DISABLE_FEATURE, &flags, sizeof flags);
    }
    return s;
}

// Sessions live for the whole process. WinHTTP keeps cookies per session handle,
// so a named jar ("anitube") is simply its own session.
std::mutex g_sessions_mutex;
std::unordered_map<std::string, HINTERNET> g_sessions;

HINTERNET session_for(const std::string& jar) {
    std::lock_guard lock(g_sessions_mutex);
    auto it = g_sessions.find(jar);
    if (it != g_sessions.end()) return it->second;
    HINTERNET s = open_session(!jar.empty());
    if (s) g_sessions.emplace(jar, s);
    return s;
}

// Sink receives response body chunks; returns false to abort.
template <typename Sink>
anime::http::Response perform(const std::wstring& method, const std::string& url, const std::string* body,
                              const std::map<std::string, std::string>& headers, int timeout_sec,
                              const std::string& jar, Sink&& sink) {
    anime::http::Response resp;
    const std::wstring wurl = widen(url);

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof uc;
    uc.dwHostNameLength = static_cast<DWORD>(-1);
    uc.dwUrlPathLength = static_cast<DWORD>(-1);
    uc.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &uc)) {
        resp.error = "URL using bad/illegal format";
        return resp;
    }
    const std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    if (path.empty()) path = L"/";
    const bool https = uc.nScheme == INTERNET_SCHEME_HTTPS;

    HINTERNET session = session_for(jar);
    if (!session) {
        resp.error = win_error(GetLastError());
        return resp;
    }
    Handle conn(WinHttpConnect(session, host.c_str(), uc.nPort, 0));
    if (!conn) {
        resp.error = win_error(GetLastError());
        return resp;
    }
    Handle req(WinHttpOpenRequest(conn.h, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0));
    if (!req) {
        resp.error = win_error(GetLastError());
        return resp;
    }
    const int ms = timeout_sec * 1000;
    WinHttpSetTimeouts(req.h, ms, ms, ms, ms);
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS; // follow https -> http too, like curl -L
    WinHttpSetOption(req.h, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof redirect);

    std::wstring header_block;
    bool has_content_type = false;
    for (const auto& [k, v] : headers) {
        if (_stricmp(k.c_str(), "Content-Type") == 0) has_content_type = true;
        // A provider's own User-Agent header replaces the session default (FLAG_REPLACE)
        header_block += widen(k) + L": " + widen(v) + L"\r\n";
    }
    if (body && !has_content_type) header_block += L"Content-Type: application/x-www-form-urlencoded\r\n";
    if (!header_block.empty())
        WinHttpAddRequestHeaders(req.h, header_block.c_str(), static_cast<DWORD>(-1L),
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    LPVOID data = body ? const_cast<char*>(body->data()) : WINHTTP_NO_REQUEST_DATA;
    const DWORD len = body ? static_cast<DWORD>(body->size()) : 0;
    if (!WinHttpSendRequest(req.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, data, len, len, 0) ||
        !WinHttpReceiveResponse(req.h, nullptr)) {
        resp.error = win_error(GetLastError());
        return resp;
    }

    DWORD status = 0, size = sizeof status;
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);
    resp.status_code = static_cast<int>(status);

    char buf[64 * 1024];
    for (;;) {
        DWORD got = 0;
        if (!WinHttpReadData(req.h, buf, sizeof buf, &got)) {
            resp.error = win_error(GetLastError());
            break;
        }
        if (got == 0) break;
        if (!sink(buf, got)) {
            resp.error = "Write error";
            break;
        }
    }
    return resp;
}

} // namespace

namespace anime::http {

Response Client::get(const std::string& url, const std::map<std::string, std::string>& headers, int timeout_sec,
                     const std::string& cookie_jar) {
    std::string out;
    Response r = perform(L"GET", url, nullptr, headers, timeout_sec, cookie_jar, [&](const char* p, DWORD n) {
        out.append(p, n);
        return true;
    });
    r.body = std::move(out);
    return r;
}

Response Client::post(const std::string& url, const std::string& data, const std::map<std::string, std::string>& headers,
                      int timeout_sec, const std::string& cookie_jar) {
    std::string out;
    Response r = perform(L"POST", url, &data, headers, timeout_sec, cookie_jar, [&](const char* p, DWORD n) {
        out.append(p, n);
        return true;
    });
    r.body = std::move(out);
    return r;
}

std::future<Response> Client::get_async(std::string url, std::map<std::string, std::string> headers, int timeout_sec,
                                        std::string cookie_jar) {
    return std::async(std::launch::async, [url = std::move(url), headers = std::move(headers), timeout_sec,
                                           cookie_jar = std::move(cookie_jar)]() {
        return get(url, headers, timeout_sec, cookie_jar);
    });
}

std::future<Response> Client::post_async(std::string url, std::string data, std::map<std::string, std::string> headers,
                                         int timeout_sec, std::string cookie_jar) {
    return std::async(std::launch::async, [url = std::move(url), data = std::move(data), headers = std::move(headers),
                                           timeout_sec, cookie_jar = std::move(cookie_jar)]() {
        return post(url, data, headers, timeout_sec, cookie_jar);
    });
}

bool Client::download_file(const std::string& url, const std::string& dest_path, int timeout_sec) {
    FILE* fp = _wfopen(widen(dest_path).c_str(), L"wb");
    if (!fp) return false;
    Response r = perform(L"GET", url, nullptr, {}, timeout_sec, {}, [&](const char* p, DWORD n) {
        return std::fwrite(p, 1, n, fp) == n;
    });
    std::fclose(fp);
    // Don't cache a 404/403 error page as if it were an image
    return r.error.empty() && r.success();
}

} // namespace anime::http
