use crate::constants as c;
use crate::md::agg::TickAgg;
use crate::rng::XorShift64;

pub struct MarketDataBench {
    pub symbol: Vec<i32>,
    pub price: Vec<i32>,
    pub qty: Vec<i64>,
    pub aggs: Vec<TickAgg>,
    pub checksum: u64,
}

impl MarketDataBench {
    pub fn new(seed: u64, n_ticks: usize, n_symbols: i32) -> Self {
        let mut symbol = vec![0; n_ticks];
        let mut price = vec![0; n_ticks];
        let mut qty = vec![0; n_ticks];
        let mut rng = XorShift64::new(seed);
        for i in 0..n_ticks {
            symbol[i] = rng.next_bounded(n_symbols as u32) as i32;
            price[i] = c::PRICE_MID + rng.next_bounded(c::PRICE_SPAN as u32) as i32
                - c::PRICE_SPAN / 2;
            qty[i] = 1 + i64::from(rng.next_bounded(100));
        }
        Self {
            symbol,
            price,
            qty,
            aggs: vec![TickAgg::default(); n_symbols as usize],
            checksum: 0,
        }
    }

    pub fn apply(&mut self, i: usize) {
        let s = self.symbol[i] as usize;
        let px = i64::from(self.price[i]);
        let q = self.qty[i];
        self.aggs[s].on_tick(px, q);
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
