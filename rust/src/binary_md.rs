use crate::c;
use crate::rng::XorShift64;
use crate::tick_agg::TickAgg;

pub const REC: usize = 32;

pub struct BinaryMdBench {
    pub arena: Vec<u8>,
    pub aggs: Vec<TickAgg>,
    pub checksum: u64,
}

impl BinaryMdBench {
    pub fn new(seed: u64, n_ticks: usize, n_symbols: i32) -> Self {
        let mut arena = vec![0u8; n_ticks * REC];
        let mut rng = XorShift64::new(seed);
        let mut o = 0usize;
        for i in 0..n_ticks {
            let symbol = rng.next_bounded(n_symbols as u32) as i32;
            let price = c::PRICE_MID + rng.next_bounded(c::PRICE_SPAN as u32) as i32
                - c::PRICE_SPAN / 2;
            let qty = 1 + i64::from(rng.next_bounded(100));
            write_u32(&mut arena, o, symbol as u32);
            write_u32(&mut arena, o + 4, 0);
            write_i64(&mut arena, o + 8, i64::from(price));
            write_i64(&mut arena, o + 16, qty);
            write_i64(&mut arena, o + 24, i as i64);
            o += REC;
        }
        Self {
            arena,
            aggs: vec![TickAgg::default(); n_symbols as usize],
            checksum: 0,
        }
    }

    pub fn apply(&mut self, i: usize) {
        let o = i * REC;
        let symbol = read_u32(&self.arena, o) as usize;
        let price = read_i64(&self.arena, o + 8);
        let qty = read_i64(&self.arena, o + 16);
        self.aggs[symbol].on_tick(price, qty);
    }

    pub fn reset_aggs(&mut self) {
        for a in &mut self.aggs {
            a.reset();
        }
        self.checksum = 0;
    }

    pub fn finish(&mut self) {
        let mut checksum = self.checksum;
        for a in &self.aggs {
            checksum = a.mix(checksum);
        }
        self.checksum = checksum;
    }
}

fn write_u32(a: &mut [u8], o: usize, v: u32) {
    a[o..o + 4].copy_from_slice(&v.to_le_bytes());
}

fn write_i64(a: &mut [u8], o: usize, v: i64) {
    a[o..o + 8].copy_from_slice(&v.to_le_bytes());
}

fn read_u32(a: &[u8], o: usize) -> u32 {
    u32::from_le_bytes(a[o..o + 4].try_into().unwrap())
}

fn read_i64(a: &[u8], o: usize) -> i64 {
    i64::from_le_bytes(a[o..o + 8].try_into().unwrap())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::market_data::MarketDataBench;

    #[test]
    fn checksum_matches_market_data() {
        let mut md = MarketDataBench::new(c::DEFAULT_SEED, 2_000, 8);
        let mut bin = BinaryMdBench::new(c::DEFAULT_SEED, 2_000, 8);
        for i in 0..2_000 {
            md.apply(i);
            bin.apply(i);
        }
        md.finish();
        bin.finish();
        assert_eq!(md.checksum, bin.checksum);
    }
}
