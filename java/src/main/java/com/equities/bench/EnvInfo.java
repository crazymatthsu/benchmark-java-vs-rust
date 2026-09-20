package com.equities.bench;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Locale;

public final class EnvInfo {
    public final String os;
    public final String arch;
    public final int cpus;
    public final long memBytes;
    public final String osPretty;
    public final String cpuModel;
    public final String runtime;

    public EnvInfo() {
        this.os = System.getProperty("os.name", "unknown");
        this.arch = System.getProperty("os.arch", "unknown");
        this.cpus = Runtime.getRuntime().availableProcessors();
        this.memBytes = readMemTotal();
        this.osPretty = readPrettyName();
        this.cpuModel = readCpuModel();
        this.runtime = System.getProperty("java.vm.name", "Java")
                + " " + System.getProperty("java.version", "?")
                + " (" + System.getProperty("java.vm.version", "?") + ")";
    }

    private static long readMemTotal() {
        try {
            List<String> lines = Files.readAllLines(Path.of("/proc/meminfo"), StandardCharsets.UTF_8);
            for (String line : lines) {
                if (line.startsWith("MemTotal:")) {
                    String num = line.replaceAll("[^0-9]", "");
                    if (!num.isEmpty()) {
                        return Long.parseLong(num) * 1024L;
                    }
                }
            }
        } catch (IOException ignored) {
            // not Linux
        }
        return Runtime.getRuntime().maxMemory();
    }

    private static String readPrettyName() {
        try {
            List<String> lines = Files.readAllLines(Path.of("/etc/os-release"), StandardCharsets.UTF_8);
            for (String line : lines) {
                if (line.startsWith("PRETTY_NAME=")) {
                    return line.substring("PRETTY_NAME=".length()).replace("\"", "").trim();
                }
            }
        } catch (IOException ignored) {
            // fall through
        }
        return System.getProperty("os.name", "unknown") + " " + System.getProperty("os.version", "");
    }

    private static String readCpuModel() {
        try {
            List<String> lines = Files.readAllLines(Path.of("/proc/cpuinfo"), StandardCharsets.UTF_8);
            String model = findPrefixed(lines, "model name");
            if (model == null) {
                model = findPrefixed(lines, "Hardware");
            }
            if (model == null) {
                String impl = findPrefixed(lines, "CPU implementer");
                String part = findPrefixed(lines, "CPU part");
                if (impl != null || part != null) {
                    model = ("implementer " + n(impl) + " part " + n(part)).trim();
                }
            }
            if (model != null) {
                return model;
            }
        } catch (IOException ignored) {
            // fall through
        }
        return System.getProperty("os.arch", "unknown");
    }

    private static String n(String s) {
        return s == null ? "?" : s;
    }

    private static String findPrefixed(List<String> lines, String key) {
        String prefix = key.toLowerCase(Locale.ROOT);
        for (String line : lines) {
            int colon = line.indexOf(':');
            if (colon <= 0) {
                continue;
            }
            String k = line.substring(0, colon).trim().toLowerCase(Locale.ROOT);
            if (k.equals(prefix)) {
                return line.substring(colon + 1).trim();
            }
        }
        return null;
    }
}
