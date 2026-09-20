use crate::constants as c;
use crate::rng::XorShift64;

pub struct Workload {
    pub op: Vec<u8>,
    pub order_id: Vec<i32>,
    pub symbol: Vec<i32>,
    pub side: Vec<i32>,
    pub price: Vec<i32>,
    pub qty: Vec<i64>,
    pub tif: Vec<u8>,
    pub account: Vec<i32>,
    #[allow(dead_code)]
    pub n_ops: usize,
}

impl Workload {
    pub fn generate(seed: u64, n_ops: usize, n_symbols: i32, n_accounts: i32) -> Self {
        let mut w = Self {
            op: vec![0; n_ops],
            order_id: vec![0; n_ops],
            symbol: vec![0; n_ops],
            side: vec![0; n_ops],
            price: vec![0; n_ops],
            qty: vec![0; n_ops],
            tif: vec![0; n_ops],
            account: vec![0; n_ops],
            n_ops,
        };
        let mut rng = XorShift64::new(seed);
        let mut next_id: i32 = 1;
        for i in 0..n_ops {
            let r = rng.next_bounded(100);
            if r < 70 || next_id == 1 {
                w.op[i] = c::OP_ADD;
                w.order_id[i] = next_id;
                next_id += 1;
                w.symbol[i] = rng.next_bounded(n_symbols as u32) as i32;
                w.side[i] = rng.next_bounded(2) as i32;
                w.price[i] = c::PRICE_MID + rng.next_bounded(c::PRICE_SPAN as u32) as i32
                    - c::PRICE_SPAN / 2;
                w.qty[i] = 1 + i64::from(rng.next_bounded(100));
                w.tif[i] = if rng.next_bounded(10) == 0 {
                    c::TIF_IOC
                } else {
                    c::TIF_GTC
                };
                w.account[i] = rng.next_bounded(n_accounts as u32) as i32;
            } else if r < 90 {
                w.op[i] = c::OP_CANCEL;
                w.order_id[i] = 1 + rng.next_bounded((next_id - 1) as u32) as i32;
                w.account[i] = rng.next_bounded(n_accounts as u32) as i32;
            } else {
                w.op[i] = c::OP_MARKET;
                w.order_id[i] = next_id;
                next_id += 1;
                w.symbol[i] = rng.next_bounded(n_symbols as u32) as i32;
                w.side[i] = rng.next_bounded(2) as i32;
                w.price[i] = 0;
                w.qty[i] = 1 + i64::from(rng.next_bounded(100));
                w.tif[i] = c::TIF_IOC;
                w.account[i] = rng.next_bounded(n_accounts as u32) as i32;
            }
        }
        w
    }
}
