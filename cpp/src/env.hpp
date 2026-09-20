#pragma once

#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#if defined(__linux__)
#include <fstream>
#endif

struct EnvInfo {
    std::string os;
    std::string arch;
    uint32_t cpus = 1;
    uint64_t mem_bytes = 0;
    std::string os_pretty;
    std::string cpu_model;
    std::string runtime;
};

inline std::string find_prefixed(const std::string& text, const std::string& key) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string k = line.substr(0, colon);
        while (!k.empty() && std::isspace(static_cast<unsigned char>(k.back()))) {
            k.pop_back();
        }
        for (char& ch : k) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        std::string want = key;
        for (char& ch : want) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (k == want) {
            std::string v = line.substr(colon + 1);
            auto s = v.find_first_not_of(" \t");
            if (s != std::string::npos) {
                v = v.substr(s);
            }
            while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) {
                v.pop_back();
            }
            return v;
        }
    }
    return {};
}

inline std::string read_file(const char* path) {
    std::ifstream in(path);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline EnvInfo collect_env() {
    EnvInfo e;
#if defined(__linux__)
    e.os = "linux";
#else
    e.os = "unknown";
#endif
#if defined(__aarch64__)
    e.arch = "aarch64";
#elif defined(__x86_64__)
    e.arch = "x86_64";
#else
    e.arch = "unknown";
#endif
    e.cpus = std::thread::hardware_concurrency();
    if (e.cpus == 0) {
        e.cpus = 1;
    }
    std::string meminfo = read_file("/proc/meminfo");
    {
        auto pos = meminfo.find("MemTotal:");
        if (pos != std::string::npos) {
            std::string num;
            for (char ch : meminfo.substr(pos)) {
                if (std::isdigit(static_cast<unsigned char>(ch))) {
                    num.push_back(ch);
                } else if (!num.empty()) {
                    break;
                }
            }
            if (!num.empty()) {
                e.mem_bytes = std::stoull(num) * 1024ULL;
            }
        }
    }
    std::string osrel = read_file("/etc/os-release");
    auto pretty = find_prefixed(osrel, "PRETTY_NAME");
    if (!pretty.empty()) {
        if (!pretty.empty() && pretty.front() == '"') {
            pretty.erase(pretty.begin());
        }
        if (!pretty.empty() && pretty.back() == '"') {
            pretty.pop_back();
        }
        e.os_pretty = pretty;
    } else {
        e.os_pretty = e.os + " " + e.arch;
    }
    std::string cpuinfo = read_file("/proc/cpuinfo");
    e.cpu_model = find_prefixed(cpuinfo, "model name");
    if (e.cpu_model.empty()) {
        e.cpu_model = find_prefixed(cpuinfo, "Hardware");
    }
    if (e.cpu_model.empty()) {
        auto impl = find_prefixed(cpuinfo, "CPU implementer");
        auto part = find_prefixed(cpuinfo, "CPU part");
        if (!impl.empty() || !part.empty()) {
            e.cpu_model = "implementer " + (impl.empty() ? "?" : impl) + " part " + (part.empty() ? "?" : part);
        }
    }
    if (e.cpu_model.empty()) {
        e.cpu_model = e.arch;
    }
    e.runtime = std::string("g++ ") + __VERSION__ + " (-O3 -flto)";
    return e;
}
