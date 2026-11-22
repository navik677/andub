// HTTP client on WinHTTP: no extra DLLs, TLS and certificates come from the
// system (Schannel + Windows certificate store), proxy settings follow Windows.
//
// Cloudflare-fronted sites (AniStar) answer WinHTTP with a "Just a moment"
// challenge page whatever the headers or HTTP version, while curl on the same
// Schannel passes. Such requests are retried with the curl.exe that ships with
// Windows 10+ (System32), and that host goes straight to curl afterwards.
#include "http_client.hpp"

#include <windows.h>
#include <winhttp.h>

#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

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
    // HTTP/2 where the server offers it (Windows 10 1607+).
    DWORD http2 = WINHTTP_PROTOCOL_FLAG_HTTP2;
    WinHttpSetOption(s, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &http2, sizeof http2);
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

// ---- curl.exe fallback for Cloudflare challenges --------------------------

bool is_challenge(const anime::http::Response& r) {
    if (r.status_code != 403 && r.status_code != 503) return false;
    return r.body.find("Just a moment") != std::string::npos ||
           r.body.find("challenge-platform") != std::string::npos || r.body.find("cf_chl") != std::string::npos;
}

std::wstring host_of(const std::string& url) {
    const std::wstring w = widen(url);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof uc;
    uc.dwHostNameLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(w.c_str(), static_cast<DWORD>(w.size()), 0, &uc)) return {};
    return std::wstring(uc.lpszHostName, uc.dwHostNameLength);
}

std::mutex g_curl_hosts_mutex;
std::set<std::wstring> g_curl_hosts;

bool use_curl_for(const std::string& url) {
    std::lock_guard lock(g_curl_hosts_mutex);
    return g_curl_hosts.count(host_of(url)) > 0;
}

void remember_curl_host(const std::string& url) {
    std::lock_guard lock(g_curl_hosts_mutex);
    g_curl_hosts.insert(host_of(url));
}

std::wstring curl_exe() {
    wchar_t dir[MAX_PATH];
    const UINT n = GetSystemDirectoryW(dir, MAX_PATH);
    return std::wstring(dir, n) + L"\\curl.exe";
}

std::wstring temp_path(const wchar_t* prefix) {
    wchar_t dir[MAX_PATH], file[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, prefix, 0, file);
    return file;
}

// One argument for CreateProcess, quoted the way the MSVC runtime parses it
std::wstring quote_arg(const std::wstring& a) {
    std::wstring out = L"\"";
    size_t backslashes = 0;
    for (wchar_t c : a) {
        if (c == L'\\') {
            ++backslashes;
        } else if (c == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out += L'"';
            backslashes = 0;
            continue;
        } else {
            backslashes = 0;
        }
        out += c;
    }
    out.append(backslashes * 2, L'\\');
    return out + L"\"";
}

std::string read_file(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

// Runs System32\curl.exe; the body goes to out_path, the status code comes back
// on stdout. No console window.
anime::http::Response curl_request(const std::string& url, const std::string* body,
                                   const std::map<std::string, std::string>& headers, int timeout_sec,
                                   const std::string& jar, const std::wstring& out_path) {
    anime::http::Response resp;
    std::vector<std::wstring> args = {L"-s", L"-S", L"-L", L"--compressed", L"--max-time",
                                      std::to_wstring(timeout_sec), L"-o", out_path, L"-w", L"%{http_code}"};
    bool has_ua = false;
    for (const auto& [k, v] : headers) {
        if (_stricmp(k.c_str(), "User-Agent") == 0) has_ua = true;
        args.push_back(L"-H");
        args.push_back(widen(k + ": " + v));
    }
    if (!has_ua) {
        args.push_back(L"-A");
        args.push_back(kUserAgent);
    }
    std::wstring body_file;
    if (body) {
        body_file = temp_path(L"adq");
        std::ofstream(body_file, std::ios::binary).write(body->data(), static_cast<std::streamsize>(body->size()));
        args.push_back(L"--data-binary");
        args.push_back(L"@" + body_file);
    }
    if (!jar.empty()) {
        wchar_t dir[MAX_PATH];
        GetTempPathW(MAX_PATH, dir);
        const std::wstring cookies = std::wstring(dir) + L"andub-" + widen(jar) + L".cookies";
        args.push_back(L"-b");
        args.push_back(cookies);
        args.push_back(L"-c");
        args.push_back(cookies);
    }
    args.push_back(widen(url));

    std::wstring cmd = quote_arg(curl_exe());
    for (const auto& a : args) cmd += L" " + quote_arg(a);

    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE read_end = nullptr, write_end = nullptr;
    CreatePipe(&read_end, &write_end, &sa, 0);
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_end;
    si.hStdError = write_end;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    const BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(write_end);
    if (!ok) {
        CloseHandle(read_end);
        if (!body_file.empty()) DeleteFileW(body_file.c_str());
        resp.error = "curl.exe could not be started";
        return resp;
    }
    std::string out;
    char chunk[512];
    DWORD got = 0;
    while (ReadFile(read_end, chunk, sizeof chunk, &got, nullptr) && got > 0) out.append(chunk, got);
    CloseHandle(read_end);
    WaitForSingleObject(pi.hProcess, static_cast<DWORD>((timeout_sec + 5) * 1000));
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (!body_file.empty()) DeleteFileW(body_file.c_str());

    // stdout ends with the status code written by -w
    size_t digits = out.size();
    while (digits > 0 && std::isdigit(static_cast<unsigned char>(out[digits - 1]))) --digits;
    resp.status_code = std::atoi(out.c_str() + digits);
    if (exit_code != 0) resp.error = "curl: " + out.substr(0, digits);
    return resp;
}

anime::http::Response curl_text(const std::string& url, const std::string* body,
                                const std::map<std::string, std::string>& headers, int timeout_sec,
                                const std::string& jar) {
    const std::wstring out = temp_path(L"adr");
    anime::http::Response r = curl_request(url, body, headers, timeout_sec, jar, out);
    r.body = read_file(out);
    DeleteFileW(out.c_str());
    return r;
}

} // namespace

namespace anime::http {

Response Client::get(const std::string& url, const std::map<std::string, std::string>& headers, int timeout_sec,
                     const std::string& cookie_jar) {
    if (use_curl_for(url)) return curl_text(url, nullptr, headers, timeout_sec, cookie_jar);
    std::string out;
    Response r = perform(L"GET", url, nullptr, headers, timeout_sec, cookie_jar, [&](const char* p, DWORD n) {
        out.append(p, n);
        return true;
    });
    r.body = std::move(out);
    if (is_challenge(r)) {
        remember_curl_host(url);
        return curl_text(url, nullptr, headers, timeout_sec, cookie_jar);
    }
    return r;
}

Response Client::post(const std::string& url, const std::string& data, const std::map<std::string, std::string>& headers,
                      int timeout_sec, const std::string& cookie_jar) {
    if (use_curl_for(url)) return curl_text(url, &data, headers, timeout_sec, cookie_jar);
    std::string out;
    Response r = perform(L"POST", url, &data, headers, timeout_sec, cookie_jar, [&](const char* p, DWORD n) {
        out.append(p, n);
        return true;
    });
    r.body = std::move(out);
    if (is_challenge(r)) {
        remember_curl_host(url);
        return curl_text(url, &data, headers, timeout_sec, cookie_jar);
    }
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
    if (use_curl_for(url)) {
        Response r = curl_request(url, nullptr, {}, timeout_sec, {}, widen(dest_path));
        return r.error.empty() && r.success();
    }
    FILE* fp = _wfopen(widen(dest_path).c_str(), L"wb");
    if (!fp) return false;
    Response r = perform(L"GET", url, nullptr, {}, timeout_sec, {}, [&](const char* p, DWORD n) {
        return std::fwrite(p, 1, n, fp) == n;
    });
    std::fclose(fp);
    // Cloudflare challenge on a host not seen before (the page body went to the file)
    if (r.error.empty() && (r.status_code == 403 || r.status_code == 503)) {
        Response c = curl_request(url, nullptr, {}, timeout_sec, {}, widen(dest_path));
        if (c.error.empty() && c.success()) {
            remember_curl_host(url);
            return true;
        }
    }
    // Don't cache a 404/403 error page as if it were an image
    return r.error.empty() && r.success();
}

} // namespace anime::http
