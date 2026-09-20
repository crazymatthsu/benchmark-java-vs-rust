use crate::constants as c;
use crate::workload::Workload;

pub struct RiskEngine {
    pub position: Vec<Vec<i64>>,
    pub acct_notional: Vec<i64>,
    pub last_px: Vec<i64>,
    pub accepts: u64,
    pub rejects: u64,
    pub checksum: u64,
}

impl RiskEngine {
    pub fn new(n_accounts: i32, n_symbols: i32) -> Self {
        Self {
            position: vec![vec![0; n_symbols as usize]; n_accounts as usize],
            acct_notional: vec![0; n_accounts as usize],
            last_px: vec![i64::from(c::PRICE_MID); n_symbols as usize],
            accepts: 0,
            rejects: 0,
            checksum: 0,
        }
    }

    pub fn apply(&mut self, w: &Workload, i: usize) {
        if w.op[i] == c::OP_CANCEL {
            return;
        }
        let acct = w.account[i] as usize;
        let sym = w.symbol[i] as usize;
        let side = w.side[i];
        let qty = w.qty[i];
        let px = if w.price[i] == 0 {
            self.last_px[sym]
        } else {
            i64::from(w.price[i])
        };
        let signed_qty = if side == c::BUY { qty } else { -qty };
        let notional = px * qty;

        let mut reject = qty <= 0 || qty > c::MAX_ORDER_QTY;
        if !reject {
            let abs_pos = (self.position[acct][sym] + signed_qty).abs();
            if abs_pos > c::MAX_POSITION {
                reject = true;
            } else if self.acct_notional[acct] + notional > c::MAX_NOTIONAL {
                reject = true;
            } else if self.last_px[sym] > 0
                && (px - self.last_px[sym]).abs() * 10_000 > self.last_px[sym] * c::COLLAR_BPS
            {
                reject = true;
            }
        }
        if reject {
            self.rejects += 1;
        } else {
            self.position[acct][sym] += signed_qty;
            self.acct_notional[acct] += notional;
            self.last_px[sym] = px;
            self.accepts += 1;
        }
    }

    pub fn finish(&mut self) {
        self.checksum = c::mix(self.checksum, self.accepts);
        self.checksum = c::mix(self.checksum, self.rejects);
        for row in &self.position {
            for &p in row {
                self.checksum = c::mix_i64(self.checksum, p);
            }
        }
    }
}
