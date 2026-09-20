use crate::c;

#[derive(Clone)]
pub struct TickAgg {
    pub open: i64,
    pub high: i64,
    pub low: i64,
    pub close: i64,
    pub notional: i64,
    pub volume: i64,
    pub ticks: i64,
    pub bars: i64,
}

impl Default for TickAgg {
    fn default() -> Self {
        Self {
            open: 0,
            high: i64::MIN,
            low: i64::MAX,
            close: 0,
            notional: 0,
            volume: 0,
            ticks: 0,
            bars: 0,
        }
    }
}

impl TickAgg {
    pub fn reset(&mut self) {
        *self = Self::default();
    }

    pub fn on_tick(&mut self, px: i64, qty: i64) {
        if self.ticks == 0 {
            self.open = px;
        }
        if px > self.high {
            self.high = px;
        }
        if px < self.low {
            self.low = px;
        }
        self.close = px;
        self.notional += px * qty;
        self.volume += qty;
        self.ticks += 1;
        if self.ticks % 100 == 0 {
            self.bars += 1;
        }
    }

    pub fn mix(&self, mut checksum: u64) -> u64 {
        checksum = c::mix_i64(checksum, self.open);
        checksum = c::mix_i64(checksum, self.high);
        checksum = c::mix_i64(checksum, self.low);
        checksum = c::mix_i64(checksum, self.close);
        checksum = c::mix_i64(checksum, self.notional);
        checksum = c::mix_i64(checksum, self.volume);
        checksum = c::mix_i64(checksum, self.bars);
        checksum
    }
}
