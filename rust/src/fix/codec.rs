use crate::constants as c;
use crate::rng::XorShift64;

pub struct FixBench {
    pub arena: Vec<u8>,
    pub off: Vec<usize>,
    pub len: Vec<usize>,
    pub checksum: u64,
}

impl FixBench {
    pub fn new(seed: u64, n_msgs: usize, n_symbols: i32) -> Self {
        let mut tmp = Vec::with_capacity(n_msgs);
        let mut rng = XorShift64::new(seed);
        let mut total = 0usize;
        for i in 0..n_msgs {
            let cl_ord_id = (i as u64) + 1;
            let symbol = rng.next_bounded(n_symbols as u32) as i32;
            let side = rng.next_bounded(2) as i32;
            let qty = 1 + i64::from(rng.next_bounded(100));
            let price = c::PRICE_MID + rng.next_bounded(c::PRICE_SPAN as u32) as i32
                - c::PRICE_SPAN / 2;
            let msg = build_new_order_single(i as i32 + 1, cl_ord_id, symbol, side, qty, price);
            total += msg.len();
            tmp.push(msg);
        }
        let mut arena = vec![0u8; total];
        let mut off = vec![0usize; n_msgs];
        let mut len = vec![0usize; n_msgs];
        let mut cursor = 0usize;
        for i in 0..n_msgs {
            off[i] = cursor;
            len[i] = tmp[i].len();
            arena[cursor..cursor + tmp[i].len()].copy_from_slice(&tmp[i]);
            cursor += tmp[i].len();
        }
        Self {
            arena,
            off,
            len,
            checksum: 0,
        }
    }

    pub fn parse(&mut self, i: usize) {
        let start = self.off[i];
        let end = start + self.len[i];
        let mut cl_ord_id: i64 = 0;
        let mut symbol: i32 = 0;
        let mut side: i32 = 0;
        let mut qty: i64 = 0;
        let mut price: i64 = 0;
        let mut p = start;
        while p < end {
            let mut tag = 0i32;
            while p < end && self.arena[p] != b'=' {
                let ch = self.arena[p];
                if ch.is_ascii_digit() {
                    tag = tag * 10 + i32::from(ch - b'0');
                }
                p += 1;
            }
            if p < end && self.arena[p] == b'=' {
                p += 1;
            }
            let vs = p;
            while p < end && self.arena[p] != 1 {
                p += 1;
            }
            let ve = p;
            if p < end && self.arena[p] == 1 {
                p += 1;
            }
            match tag {
                11 => cl_ord_id = parse_long(&self.arena, vs, ve),
                55 => symbol = parse_symbol(&self.arena, vs, ve),
                54 => {
                    let v = parse_long(&self.arena, vs, ve);
                    side = if v == 1 { c::BUY } else { c::SELL };
                }
                38 => qty = parse_long(&self.arena, vs, ve),
                44 => price = parse_long(&self.arena, vs, ve),
                _ => {}
            }
        }
        self.checksum = c::mix_i64(self.checksum, cl_ord_id);
        self.checksum = c::mix_i32(self.checksum, symbol);
        self.checksum = c::mix_i32(self.checksum, side);
        self.checksum = c::mix_i64(self.checksum, qty);
        self.checksum = c::mix_i64(self.checksum, price);
    }
}

fn build_new_order_single(
    seq: i32,
    cl_ord_id: u64,
    symbol: i32,
    side: i32,
    qty: i64,
    price: i32,
) -> Vec<u8> {
    let body = format!(
        "35=D\x0134={seq}\x0149=SENDER\x0156=TARGET\x0111={cl_ord_id}\x0155=SYM{sym}\x0154={side}\x0138={qty}\x0144={price}\x0140=2\x0159=0\x01",
        seq = seq,
        cl_ord_id = cl_ord_id,
        sym = pad2(symbol),
        side = if side == c::BUY { 1 } else { 2 },
        qty = qty,
        price = price
    );
    let header = format!("8=FIX.4.4\x019={}\x01", body.len());
    let prefix = format!("{header}{body}");
    let sum: u32 = prefix.bytes().map(u32::from).sum();
    let msg = format!("{prefix}10={}\x01", pad3(sum % 256));
    msg.into_bytes()
}

fn parse_symbol(b: &[u8], s: usize, e: usize) -> i32 {
    let mut k = s;
    if e - s >= 3 && b[k] == b'S' && b[k + 1] == b'Y' && b[k + 2] == b'M' {
        k += 3;
    }
    parse_long(b, k, e) as i32
}

fn parse_long(b: &[u8], s: usize, e: usize) -> i64 {
    let mut v = 0i64;
    for &ch in &b[s..e] {
        if ch.is_ascii_digit() {
            v = v * 10 + i64::from(ch - b'0');
        }
    }
    v
}

fn pad2(n: i32) -> String {
    if n < 10 {
        format!("0{n}")
    } else {
        n.to_string()
    }
}

fn pad3(n: u32) -> String {
    format!("{n:03}")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn message_is_ascii_fix() {
        let m = build_new_order_single(1, 1, 3, c::BUY, 10, 10000);
        assert!(m.starts_with(b"8=FIX.4.4"));
        assert!(m.contains(&1u8));
    }
}
