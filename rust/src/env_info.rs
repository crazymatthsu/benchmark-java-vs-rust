use std::fs;

pub struct EnvInfo {
    pub os: String,
    pub arch: String,
    pub cpus: u32,
    pub mem_bytes: u64,
    pub os_pretty: String,
    pub cpu_model: String,
    pub runtime: String,
}

impl EnvInfo {
    pub fn collect() -> Self {
        let cpus = std::thread::available_parallelism()
            .map(|n| n.get() as u32)
            .unwrap_or(1);
        Self {
            os: std::env::consts::OS.to_string(),
            arch: std::env::consts::ARCH.to_string(),
            cpus,
            mem_bytes: read_mem_total(),
            os_pretty: read_pretty_name(),
            cpu_model: read_cpu_model(),
            runtime: env!("RUSTC_VERSION").to_string(),
        }
    }
}

fn read_mem_total() -> u64 {
    let Ok(text) = fs::read_to_string("/proc/meminfo") else {
        return 0;
    };
    for line in text.lines() {
        if let Some(rest) = line.strip_prefix("MemTotal:") {
            let num: String = rest.chars().filter(|c| c.is_ascii_digit()).collect();
            if let Ok(kb) = num.parse::<u64>() {
                return kb.saturating_mul(1024);
            }
        }
    }
    0
}

fn read_pretty_name() -> String {
    let Ok(text) = fs::read_to_string("/etc/os-release") else {
        return format!("{} {}", std::env::consts::OS, std::env::consts::ARCH);
    };
    for line in text.lines() {
        if let Some(v) = line.strip_prefix("PRETTY_NAME=") {
            return v.trim().trim_matches('"').to_string();
        }
    }
    std::env::consts::OS.to_string()
}

fn read_cpu_model() -> String {
    let Ok(text) = fs::read_to_string("/proc/cpuinfo") else {
        return std::env::consts::ARCH.to_string();
    };
    let lines: Vec<&str> = text.lines().collect();
    if let Some(v) = find_prefixed(&lines, "model name") {
        return v;
    }
    if let Some(v) = find_prefixed(&lines, "Hardware") {
        return v;
    }
    let impl_ = find_prefixed(&lines, "CPU implementer");
    let part = find_prefixed(&lines, "CPU part");
    if impl_.is_some() || part.is_some() {
        return format!(
            "implementer {} part {}",
            impl_.unwrap_or_else(|| "?".into()),
            part.unwrap_or_else(|| "?".into())
        );
    }
    std::env::consts::ARCH.to_string()
}

fn find_prefixed(lines: &[&str], key: &str) -> Option<String> {
    let key = key.to_ascii_lowercase();
    for line in lines {
        let Some((k, v)) = line.split_once(':') else {
            continue;
        };
        if k.trim().eq_ignore_ascii_case(&key) {
            return Some(v.trim().to_string());
        }
    }
    None
}
