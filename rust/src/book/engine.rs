use crate::constants as c;
use crate::workload::Workload;

#[derive(Clone, Default)]
pub struct Order {
    pub symbol: i32,
    pub side: i32,
    pub price: i32,
    pub qty: i64,
}

pub struct Level {
    pub ids: Vec<i32>,
    pub total_qty: i64,
}

impl Level {
    fn new() -> Self {
        Self {
            ids: Vec::with_capacity(8),
            total_qty: 0,
        }
    }

    fn add(&mut self, id: i32) {
        self.ids.push(id);
    }
}

pub struct Book {
    pub bids: Vec<Level>,
    pub asks: Vec<Level>,
    pub best_bid: i32,
    pub best_ask: i32,
}

impl Book {
    fn new() -> Self {
        let n = (c::PRICE_MAX as usize) + 1;
        let mut bids = Vec::with_capacity(n);
        let mut asks = Vec::with_capacity(n);
        for _ in 0..n {
            bids.push(Level::new());
            asks.push(Level::new());
        }
        Self {
            bids,
            asks,
            best_bid: 0,
            best_ask: c::PRICE_MAX + 1,
        }
    }
}

pub struct OrderBookEngine {
    pub orders: Vec<Order>,
    pub books: Vec<Book>,
    pub checksum: u64,
    pub fill_count: u64,
    pub fill_qty: i64,
    pub cancel_hits: u64,
    pub cancel_misses: u64,
    pub ioc_killed: i64,
    pub rest_count: u64,
}

impl OrderBookEngine {
    pub fn new(n_symbols: i32, max_order_id: i32) -> Self {
        let books = (0..n_symbols).map(|_| Book::new()).collect();
        let orders = vec![Order::default(); (max_order_id as usize) + 1];
        Self {
            orders,
            books,
            checksum: 0,
            fill_count: 0,
            fill_qty: 0,
            cancel_hits: 0,
            cancel_misses: 0,
            ioc_killed: 0,
            rest_count: 0,
        }
    }

    pub fn apply(&mut self, w: &Workload, i: usize) {
        match w.op[i] {
            c::OP_ADD => self.add(
                w.order_id[i],
                w.symbol[i],
                w.side[i],
                w.price[i],
                w.qty[i],
                w.tif[i],
            ),
            c::OP_CANCEL => self.cancel(w.order_id[i]),
            _ => self.market(w.order_id[i], w.symbol[i], w.side[i], w.qty[i]),
        }
    }

    pub fn add(&mut self, id: i32, symbol: i32, side: i32, price: i32, qty: i64, tif: u8) {
        let rem = if side == c::BUY {
            self.match_buy(id, symbol, price, qty)
        } else {
            self.match_sell(id, symbol, price, qty)
        };
        if rem > 0 && tif == c::TIF_GTC {
            self.rest(id, symbol, side, price, rem);
        } else if rem > 0 {
            self.ioc_killed += rem;
        }
    }

    pub fn market(&mut self, id: i32, symbol: i32, side: i32, qty: i64) {
        let rem = if side == c::BUY {
            self.match_buy(id, symbol, c::PRICE_MAX, qty)
        } else {
            self.match_sell(id, symbol, c::PRICE_MIN, qty)
        };
        if rem > 0 {
            self.ioc_killed += rem;
        }
    }

    pub fn cancel(&mut self, id: i32) {
        if id < 0 || (id as usize) >= self.orders.len() {
            self.cancel_misses += 1;
            return;
        }
        let o = &self.orders[id as usize];
        if o.qty <= 0 {
            self.cancel_misses += 1;
            return;
        }
        let symbol = o.symbol;
        let side = o.side;
        let price = o.price;
        let qty = o.qty;
        let book = &mut self.books[symbol as usize];
        let lvl = if side == c::BUY {
            &mut book.bids[price as usize]
        } else {
            &mut book.asks[price as usize]
        };
        lvl.total_qty -= qty;
        self.orders[id as usize].qty = 0;
        self.cancel_hits += 1;
        if lvl.total_qty <= 0 {
            if side == c::BUY && book.best_bid == price {
                while book.best_bid >= c::PRICE_MIN
                    && book.bids[book.best_bid as usize].total_qty <= 0
                {
                    book.best_bid -= 1;
                }
            } else if side == c::SELL && book.best_ask == price {
                while book.best_ask <= c::PRICE_MAX
                    && book.asks[book.best_ask as usize].total_qty <= 0
                {
                    book.best_ask += 1;
                }
            }
        }
    }

    pub fn finish(&mut self) {
        for book in &self.books {
            let mut bid = 0i64;
            let mut ask = 0i64;
            for p in c::PRICE_MIN..=c::PRICE_MAX {
                bid += book.bids[p as usize].total_qty;
                ask += book.asks[p as usize].total_qty;
            }
            self.checksum = c::mix_i64(self.checksum, bid);
            self.checksum = c::mix_i64(self.checksum, ask);
            self.checksum = c::mix(self.checksum, self.fill_count);
            self.checksum = c::mix(self.checksum, self.cancel_hits);
        }
    }

    fn rest(&mut self, id: i32, symbol: i32, side: i32, price: i32, qty: i64) {
        let o = &mut self.orders[id as usize];
        o.symbol = symbol;
        o.side = side;
        o.price = price;
        o.qty = qty;
        let book = &mut self.books[symbol as usize];
        if side == c::BUY {
            book.bids[price as usize].add(id);
            book.bids[price as usize].total_qty += qty;
            if price > book.best_bid {
                book.best_bid = price;
            }
        } else {
            book.asks[price as usize].add(id);
            book.asks[price as usize].total_qty += qty;
            if price < book.best_ask {
                book.best_ask = price;
            }
        }
        self.rest_count += 1;
    }

    fn match_buy(&mut self, taker_id: i32, symbol: i32, limit_px: i32, mut qty: i64) -> i64 {
        loop {
            let best_ask = self.books[symbol as usize].best_ask;
            if !(qty > 0 && best_ask <= c::PRICE_MAX && best_ask <= limit_px) {
                break;
            }
            qty = self.hit_level(symbol, false, best_ask, taker_id, qty);
            let book = &mut self.books[symbol as usize];
            if book.asks[best_ask as usize].total_qty <= 0 {
                book.best_ask += 1;
                while book.best_ask <= c::PRICE_MAX
                    && book.asks[book.best_ask as usize].total_qty <= 0
                {
                    book.best_ask += 1;
                }
            }
        }
        qty
    }

    fn match_sell(&mut self, taker_id: i32, symbol: i32, limit_px: i32, mut qty: i64) -> i64 {
        loop {
            let best_bid = self.books[symbol as usize].best_bid;
            if !(qty > 0 && best_bid >= c::PRICE_MIN && best_bid >= limit_px) {
                break;
            }
            qty = self.hit_level(symbol, true, best_bid, taker_id, qty);
            let book = &mut self.books[symbol as usize];
            if book.bids[best_bid as usize].total_qty <= 0 {
                book.best_bid -= 1;
                while book.best_bid >= c::PRICE_MIN
                    && book.bids[book.best_bid as usize].total_qty <= 0
                {
                    book.best_bid -= 1;
                }
            }
        }
        qty
    }

    fn hit_level(
        &mut self,
        symbol: i32,
        is_bid: bool,
        px: i32,
        taker_id: i32,
        mut qty: i64,
    ) -> i64 {
        let n = if is_bid {
            self.books[symbol as usize].bids[px as usize].ids.len()
        } else {
            self.books[symbol as usize].asks[px as usize].ids.len()
        };
        for i in 0..n {
            if qty <= 0 {
                break;
            }
            let mid = if is_bid {
                self.books[symbol as usize].bids[px as usize].ids[i]
            } else {
                self.books[symbol as usize].asks[px as usize].ids[i]
            };
            let maker_qty = self.orders[mid as usize].qty;
            if maker_qty <= 0 {
                continue;
            }
            let fill = qty.min(maker_qty);
            self.orders[mid as usize].qty -= fill;
            qty -= fill;
            if is_bid {
                self.books[symbol as usize].bids[px as usize].total_qty -= fill;
            } else {
                self.books[symbol as usize].asks[px as usize].total_qty -= fill;
            }
            self.fill_count += 1;
            self.fill_qty += fill;
            self.checksum = c::mix_i32(self.checksum, taker_id);
            self.checksum = c::mix_i32(self.checksum, mid);
            self.checksum = c::mix_i32(self.checksum, px);
            self.checksum = c::mix_i64(self.checksum, fill);
        }
        qty
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::workload::Workload;

    #[test]
    fn matching_buy_hits_resting_sell() {
        let mut eng = OrderBookEngine::new(1, 8);
        eng.add(1, 0, c::SELL, 10_000, 10, c::TIF_GTC);
        eng.add(2, 0, c::BUY, 10_000, 10, c::TIF_GTC);
        assert_eq!(eng.fill_count, 1);
        assert_eq!(eng.fill_qty, 10);
        assert_eq!(eng.orders[1].qty, 0);
        assert_eq!(eng.orders[2].qty, 0);
    }

    #[test]
    fn cancel_removes_liquidity() {
        let mut eng = OrderBookEngine::new(1, 8);
        eng.add(1, 0, c::SELL, 10_000, 5, c::TIF_GTC);
        eng.cancel(1);
        eng.add(2, 0, c::BUY, 10_000, 5, c::TIF_IOC);
        assert_eq!(eng.fill_count, 0);
        assert_eq!(eng.cancel_hits, 1);
        assert_eq!(eng.ioc_killed, 5);
    }

    #[test]
    fn same_workload_is_deterministic() {
        let w = Workload::generate(c::DEFAULT_SEED, 5_000, 8, 16);
        let mut a = OrderBookEngine::new(8, 5_000);
        let mut b = OrderBookEngine::new(8, 5_000);
        for i in 0..w.n_ops {
            a.apply(&w, i);
            b.apply(&w, i);
        }
        a.finish();
        b.finish();
        assert_eq!(a.checksum, b.checksum);
        assert_eq!(a.fill_count, b.fill_count);
        assert!(a.fill_count > 0);
    }
}
