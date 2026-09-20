//! Equities trading microbenchmarks (Rust).
//!
//! Domain modules mirror a small trading stack. The binary in `main.rs` is only
//! the CLI harness.

pub mod book;
pub mod constants;
pub mod env;
pub mod fix;
pub mod harness;
pub mod md;
pub mod rng;
pub mod risk;
pub mod workload;

pub use harness::run;
