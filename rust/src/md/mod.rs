//! Market-data aggregation: in-memory ticks and packed binary records.

mod agg;
mod binary;
mod ticks;

pub use agg::TickAgg;
pub use binary::{BinaryMdBench, REC};
pub use ticks::MarketDataBench;
