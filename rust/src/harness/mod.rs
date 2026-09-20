//! CLI, timers, JSON output.

mod cli;
mod json;
mod percentiles;

pub use cli::run;
pub use json::{write, BenchResult};
