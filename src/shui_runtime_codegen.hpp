#ifndef SHAY_SHUI_RUNTIME_CODEGEN_HPP
#define SHAY_SHUI_RUNTIME_CODEGEN_HPP

#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace shay::ui::runtime_codegen {
inline std::string cpp_quote(const std::string& value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += static_cast<char>(c); break;
        }
    }
    out += "\"";
    return out;
}

inline void emit(std::ostream& out, const std::string& token,
                 const std::vector<std::pair<std::string, bool>>& events) {
    out << "static const std::string shay_ui_token=" << cpp_quote(token) << ";\n"
        << "static std::mutex shay_ui_mutex;\n"
        << "static std::string shay_ui_dispatch(const std::string& method){"
           "std::lock_guard<std::mutex> guard(shay_ui_mutex);"
           "std::string output;shay_ui_capture=&output;try{";
    for (const auto& event : events) {
        out << "if(method==" << cpp_quote(event.first) << "){";
        if (event.second) {
            out << "auto value=shay_fn_" << event.first
                << "(shay_ui_values);std::ostringstream result;"
                   "result<<std::setprecision(15)<<value;output+=result.str();";
        } else {
            out << "shay_fn_" << event.first << "(shay_ui_values);";
        }
        out << "}else ";
    }
    out << "{output=\"unknown or unbound Shay UI method: \"+method;}"
           "}catch(const std::exception& error){"
           "output=std::string(\"Shay UI method failed: \")+error.what();}"
           "shay_ui_capture=nullptr;return output;}\n";

    out << R"SHAY(
static std::string shay_ui_json_escape(const std::string& value) {
    std::string out;
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 32) {
                    const char* hex = "0123456789abcdef";
                    out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15];
                } else out += static_cast<char>(c);
        }
    }
    return out;
}
static std::string shay_ui_url_decode(const std::string& value) {
    std::string out;
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            int a = hex(value[i + 1]), b = hex(value[i + 2]);
            if (a >= 0 && b >= 0) {
                out.push_back(static_cast<char>((a << 4) | b));
                i += 2;
                continue;
            }
        }
        out.push_back(value[i] == '+' ? ' ' : value[i]);
    }
    return out;
}
static std::string shay_ui_state() {
    std::lock_guard<std::mutex> guard(shay_ui_mutex);
    std::ostringstream out;
    out << "{";
    bool first = true;
    for (const auto& item : shay_ui_values) {
        if (!first) out << ",";
        first = false;
        out << "\"" << shay_ui_json_escape(item.first) << "\":\""
            << shay_ui_json_escape(item.second) << "\"";
    }
    out << "}";
    return out.str();
}
#ifdef _WIN32
using ShayUiSocket = SOCKET;
static void shay_ui_close(ShayUiSocket socket) { closesocket(socket); }
#else
using ShayUiSocket = int;
static void shay_ui_close(ShayUiSocket socket) { close(socket); }
#endif
static void shay_ui_reply(ShayUiSocket client, int status,
                          const std::string& type, const std::string& body) {
    const std::string label = status == 200 ? "OK" :
        status == 204 ? "No Content" : status == 403 ? "Forbidden" :
        status == 404 ? "Not Found" : "Bad Request";
    std::ostringstream response;
    response << "HTTP/1.1 " << status << " " << label
             << "\r\nAccess-Control-Allow-Origin: *"
             << "\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS"
             << "\r\nAccess-Control-Allow-Headers: Content-Type"
             << "\r\nAccess-Control-Allow-Private-Network: true"
             << "\r\nContent-Type: " << type
             << "\r\nContent-Length: " << body.size()
             << "\r\nConnection: close\r\n\r\n" << body;
    const std::string bytes = response.str();
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        int count = send(client, bytes.data() + sent,
                         static_cast<int>(bytes.size() - sent), 0);
        if (count <= 0) break;
        sent += static_cast<std::size_t>(count);
    }
}
static int shay_ui_serve() {
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "Shay UI: Winsock initialization failed\n";
        return 1;
    }
#endif
    ShayUiSocket server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
    if (server == INVALID_SOCKET) return 1;
#else
    if (server < 0) return 1;
#endif
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(18765);
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(server, 16) != 0) {
        std::cerr << "Shay UI: cannot bind http://127.0.0.1:18765 (port may be in use)\n";
        shay_ui_close(server);
        return 1;
    }
    std::cout << "Shay UI bridge listening at http://127.0.0.1:18765\n";
    for (;;) {
        ShayUiSocket client = accept(server, nullptr, nullptr);
#ifdef _WIN32
        if (client == INVALID_SOCKET) continue;
#else
        if (client < 0) continue;
#endif
        std::string request;
        char buffer[4096];
        std::size_t expected = 0;
        std::size_t header_end = std::string::npos;
        for (;;) {
            int count = recv(client, buffer, sizeof(buffer), 0);
            if (count <= 0) break;
            request.append(buffer, static_cast<std::size_t>(count));
            if (header_end == std::string::npos) {
                header_end = request.find("\r\n\r\n");
                if (header_end != std::string::npos) {
                    auto at = request.find("Content-Length:");
                    if (at != std::string::npos && at < header_end) {
                        at += 15;
                        while (at < request.size() && request[at] == ' ') ++at;
                        expected = static_cast<std::size_t>(
                            std::strtoul(request.c_str() + at, nullptr, 10));
                    }
                }
            }
            if (header_end != std::string::npos &&
                request.size() >= header_end + 4 + expected) break;
            if (request.size() > 65536) break;
        }
        if (header_end == std::string::npos) {
            shay_ui_reply(client, 400, "text/plain", "bad request");
            shay_ui_close(client);
            continue;
        }
        const auto first_end = request.find("\r\n");
        const std::string first = request.substr(0, first_end);
        const auto first_space = first.find(' ');
        const auto second_space = first.find(' ', first_space + 1);
        const std::string verb = first.substr(0, first_space);
        const std::string path = first_space == std::string::npos ? "" :
            first.substr(first_space + 1, second_space - first_space - 1);
        const std::string body = request.substr(header_end + 4, expected);
        if (verb == "OPTIONS") {
            shay_ui_reply(client, 204, "text/plain", "");
        } else if (verb == "GET" && path.rfind("/state?token=", 0) == 0) {
            if (shay_ui_url_decode(path.substr(13)) == shay_ui_token)
                shay_ui_reply(client, 200, "application/json", shay_ui_state());
            else shay_ui_reply(client, 403, "text/plain", "invalid token");
        } else if (verb == "POST" && (path == "/event" || path == "/set")) {
            const auto split = body.find('|');
            if (split == std::string::npos || body.substr(0, split) != shay_ui_token) {
                shay_ui_reply(client, 403, "text/plain", "invalid token");
            } else if (path == "/event") {
                shay_ui_reply(client, 200, "text/plain",
                              shay_ui_dispatch(body.substr(split + 1)));
            } else {
                const std::string change = body.substr(split + 1);
                const auto equals = change.find('=');
                if (equals == std::string::npos) {
                    shay_ui_reply(client, 400, "text/plain", "expected name=value");
                } else {
                    std::lock_guard<std::mutex> guard(shay_ui_mutex);
                    const std::string name = shay_ui_url_decode(change.substr(0, equals));
                    auto existing = shay_ui_values.find(name);
                    if (existing == shay_ui_values.end()) {
                        shay_ui_reply(client, 404, "text/plain", "unknown Shay value");
                    } else {
                        existing->second = shay_ui_url_decode(change.substr(equals + 1));
                        shay_ui_reply(client, 200, "text/plain", "ok");
                    }
                }
            }
        } else {
            shay_ui_reply(client, 404, "text/plain", "not found");
        }
        shay_ui_close(client);
    }
    return 0;
}
)SHAY";
}
} // namespace shay::ui::runtime_codegen

#endif
