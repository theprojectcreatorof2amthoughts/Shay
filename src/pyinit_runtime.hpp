#ifndef SHAY_PYINIT_RUNTIME_HPP
#define SHAY_PYINIT_RUNTIME_HPP

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace shay {
class PythonSession {
public:
    PythonSession() = default;
    PythonSession(const PythonSession&) = delete;
    PythonSession& operator=(const PythonSession&) = delete;
    ~PythonSession() { stop(); }

    bool start(const std::filesystem::path& runtime, std::string& error) {
        if (running_) return true;
        const auto executable = runtime / "python.exe";
        const auto host = runtime / "pyhost.py";
        if (!std::filesystem::is_regular_file(executable) ||
            !std::filesystem::is_regular_file(host)) {
            error = "pyinit runtime is incomplete (expected runtime/python.exe and runtime/pyhost.py)";
            return false;
        }
#ifdef _WIN32
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE child_input_read = nullptr, parent_input_write = nullptr;
        HANDLE parent_output_read = nullptr, child_output_write = nullptr;
        if (!CreatePipe(&child_input_read, &parent_input_write, &security, 0) ||
            !CreatePipe(&parent_output_read, &child_output_write, &security, 0)) {
            error = "cannot create Python runtime pipes";
            close_handle(child_input_read); close_handle(parent_input_write);
            close_handle(parent_output_read); close_handle(child_output_write);
            return false;
        }
        SetHandleInformation(parent_input_write, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(parent_output_read, HANDLE_FLAG_INHERIT, 0);
        STARTUPINFOA startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = child_input_read;
        startup.hStdOutput = child_output_write;
        startup.hStdError = child_output_write;
        PROCESS_INFORMATION process{};
        std::string command = quote(executable.string()) + " " + quote(host.string());
        std::vector<char> mutable_command(command.begin(), command.end());
        mutable_command.push_back('\0');
        const std::string working_directory = runtime.string();
        const BOOL created = CreateProcessA(nullptr, mutable_command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, working_directory.c_str(), &startup, &process);
        CloseHandle(child_input_read);
        CloseHandle(child_output_write);
        if (!created) {
            error = "cannot start embedded Python runtime (Windows error " +
                    std::to_string(GetLastError()) + ")";
            CloseHandle(parent_input_write);
            CloseHandle(parent_output_read);
            return false;
        }
        CloseHandle(process.hThread);
        process_ = process.hProcess;
        input_ = parent_input_write;
        output_ = parent_output_read;
#else
        int input_pipe[2]{-1, -1}, output_pipe[2]{-1, -1};
        if (pipe(input_pipe) != 0 || pipe(output_pipe) != 0) {
            error = "cannot create Python runtime pipes";
            if (input_pipe[0] >= 0) close(input_pipe[0]);
            if (input_pipe[1] >= 0) close(input_pipe[1]);
            if (output_pipe[0] >= 0) close(output_pipe[0]);
            if (output_pipe[1] >= 0) close(output_pipe[1]);
            return false;
        }
        const pid_t child = fork();
        if (child < 0) {
            error = "cannot start embedded Python runtime";
            close(input_pipe[0]); close(input_pipe[1]);
            close(output_pipe[0]); close(output_pipe[1]);
            return false;
        }
        if (child == 0) {
            dup2(input_pipe[0], STDIN_FILENO);
            dup2(output_pipe[1], STDOUT_FILENO);
            close(input_pipe[0]); close(input_pipe[1]);
            close(output_pipe[0]); close(output_pipe[1]);
            if (chdir(runtime.c_str()) != 0) _exit(126);
            execl(executable.c_str(), executable.c_str(), host.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        close(input_pipe[0]); close(output_pipe[1]);
        child_ = child;
        input_ = input_pipe[1];
        output_ = output_pipe[0];
#endif
        running_ = true;
        return true;
    }

    bool execute(const std::string& code, std::string& error,
                 const std::function<std::string(const std::string&)>& input_handler,
                 const std::function<void(const std::string&)>& output_handler) {
        if (!running_) { error = "Python runtime has not been started"; return false; }
        const std::string request = encode_hex(code) + "\n";
        if (!write_all(request)) { error = "cannot send code to embedded Python runtime"; stop(); return false; }
        for (;;) {
            std::string response;
            if (!read_line(response)) { error = "embedded Python runtime stopped unexpectedly"; stop(); return false; }
            if (!response.empty() && (response[0] == 'I' || response[0] == 'P')) {
                std::string payload;
                if (!decode_hex(response.substr(1), payload)) {
                    error = "embedded Python runtime sent malformed protocol data"; stop(); return false;
                }
                if (response[0] == 'P') {
                    if (output_handler) output_handler(payload);
                } else {
                    const std::string entered = input_handler ? input_handler(payload) : std::string();
                    if (!write_all(encode_hex(entered) + "\n")) {
                        error = "cannot send input to embedded Python runtime"; stop(); return false;
                    }
                }
                continue;
            }
            if (response != "O" && response != "E") {
                error = "embedded Python runtime returned an invalid response"; stop(); return false;
            }
            if (response == "E") {
                error = "Python block raised an exception (see traceback above)";
                return false;
            }
            return true;
        }
    }

    void stop() {
        if (!running_) return;
#ifdef _WIN32
        if (input_) { CloseHandle(input_); input_ = nullptr; }
        if (output_) { CloseHandle(output_); output_ = nullptr; }
        if (process_) {
            WaitForSingleObject(process_, 1000);
            DWORD code = 0;
            if (GetExitCodeProcess(process_, &code) && code == STILL_ACTIVE) {
                TerminateProcess(process_, 1);
                WaitForSingleObject(process_, 1000);
            }
            CloseHandle(process_); process_ = nullptr;
        }
#else
        if (input_ >= 0) { close(input_); input_ = -1; }
        if (output_ >= 0) { close(output_); output_ = -1; }
        if (child_ > 0) {
            int status = 0;
            if (waitpid(child_, &status, WNOHANG) == 0) {
                kill(child_, SIGTERM);
                waitpid(child_, &status, 0);
            }
            child_ = -1;
        }
#endif
        running_ = false;
    }

private:
    bool running_ = false;
#ifdef _WIN32
    HANDLE input_ = nullptr, output_ = nullptr, process_ = nullptr;
    static void close_handle(HANDLE& handle) {
        if (handle) { CloseHandle(handle); handle = nullptr; }
    }
    static std::string quote(const std::string& value) {
        std::string result = "\"";
        for (char ch : value) {
            if (ch == '"') result += "\\\"";
            else result += ch;
        }
        result += "\"";
        return result;
    }
#else
    int input_ = -1, output_ = -1;
    pid_t child_ = -1;
#endif
    static std::string encode_hex(const std::string& value) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string result;
        result.reserve(value.size() * 2);
        for (unsigned char byte : value) {
            result.push_back(digits[byte >> 4]);
            result.push_back(digits[byte & 15]);
        }
        return result;
    }
    static bool decode_hex(const std::string& value, std::string& result) {
        if (value.size() % 2) return false;
        auto digit = [](char ch) -> int {
            if (ch >= '0' && ch <= '9') return ch - '0';
            if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
            return -1;
        };
        result.clear();
        result.reserve(value.size() / 2);
        for (std::size_t i = 0; i < value.size(); i += 2) {
            const int high = digit(value[i]), low = digit(value[i + 1]);
            if (high < 0 || low < 0) return false;
            result.push_back(static_cast<char>((high << 4) | low));
        }
        return true;
    }
    bool write_all(const std::string& value) {
        std::size_t offset = 0;
        while (offset < value.size()) {
#ifdef _WIN32
            DWORD written = 0;
            if (!WriteFile(input_, value.data() + offset,
                           static_cast<DWORD>(value.size() - offset), &written, nullptr) || written == 0)
                return false;
            offset += written;
#else
            const ssize_t written = write(input_, value.data() + offset, value.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) return false;
            offset += static_cast<std::size_t>(written);
#endif
        }
        return true;
    }
    bool read_line(std::string& value) {
        value.clear();
        for (;;) {
            char ch = 0;
#ifdef _WIN32
            DWORD count = 0;
            if (!ReadFile(output_, &ch, 1, &count, nullptr) || count == 0) return false;
#else
            const ssize_t count = read(output_, &ch, 1);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) return false;
#endif
            if (ch == '\n') return true;
            value.push_back(ch);
        }
    }
};
}
#endif
