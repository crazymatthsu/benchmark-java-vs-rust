pub const PRICE_MIN: i32 = 1;
pub const PRICE_MAX: i32 = 20_000;
pub const PRICE_MID: i32 = 10_000;
pub const PRICE_SPAN: i32 = 200;
pub const MIX: u64 = 1_000_003;
pub const DEFAULT_SEED: u64 = 0x0C0FFEE123456789;

pub const BUY: i32 = 0;
pub const SELL: i32 = 1;
pub const TIF_GTC: u8 = 0;
pub const TIF_IOC: u8 = 1;
pub const OP_ADD: u8 = 0;
pub const OP_CANCEL: u8 = 1;
pub const OP_MARKET: u8 = 2;

pub const MAX_ORDER_QTY: i64 = 10_000;
pub const MAX_POSITION: i64 = 50_000;
pub const MAX_NOTIONAL: i64 = 1_000_000_000_000;
pub const COLLAR_BPS: i64 = 200;

#[inline]
pub fn mix(checksum: u64, x: u64) -> u64 {
    checksum.wrapping_mul(MIX).wrapping_add(x)
}

#[inline]
pub fn mix_i64(checksum: u64, x: i64) -> u64 {
    mix(checksum, x as u64)
}

#[inline]
pub fn mix_i32(checksum: u64, x: i32) -> u64 {
    mix(checksum, x as u64)
}
