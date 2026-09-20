fn main() {
    let version = std::process::Command::new("rustc")
        .arg("--version")
        .output()
        .ok()
        .and_then(|o| String::from_utf8(o.stdout).ok())
        .unwrap_or_else(|| "rustc unknown".to_string());
    println!("cargo:rustc-env=RUSTC_VERSION={}", version.trim());
}
