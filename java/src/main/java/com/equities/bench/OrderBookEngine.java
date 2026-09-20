package com.equities.bench;

import java.util.Arrays;

public final class OrderBookEngine {
    public static final class Order {
        int symbol;
        int side;
        int price;
        long qty;
    }

    public static final class Level {
        int[] ids = new int[8];
        int size;
        long totalQty;

        void add(int id) {
            if (size == ids.length) {
                ids = Arrays.copyOf(ids, ids.length * 2);
            }
            ids[size++] = id;
        }
    }

    public static final class Book {
        final Level[] bids = new Level[C.PRICE_MAX + 1];
        final Level[] asks = new Level[C.PRICE_MAX + 1];
        int bestBid = 0;
        int bestAsk = C.PRICE_MAX + 1;

        Book() {
            for (int i = 0; i <= C.PRICE_MAX; i++) {
                bids[i] = new Level();
                asks[i] = new Level();
            }
        }
    }

    public final Order[] orders;
    public final Book[] books;
    public long checksum;
    public long fillCount;
    public long fillQty;
    public long cancelHits;
    public long cancelMisses;
    public long iocKilled;
    public long restCount;

    public OrderBookEngine(int nSymbols, int maxOrderId) {
        books = new Book[nSymbols];
        for (int i = 0; i < nSymbols; i++) {
            books[i] = new Book();
        }
        orders = new Order[maxOrderId + 1];
        for (int i = 0; i < orders.length; i++) {
            orders[i] = new Order();
        }
    }

    public void apply(Workload w, int i) {
        byte op = w.op[i];
        if (op == C.OP_ADD) {
            add(w.orderId[i], w.symbol[i], w.side[i], w.price[i], w.qty[i], w.tif[i]);
        } else if (op == C.OP_CANCEL) {
            cancel(w.orderId[i]);
        } else {
            market(w.orderId[i], w.symbol[i], w.side[i], w.qty[i]);
        }
    }

    public void add(int id, int symbol, int side, int price, long qty, byte tif) {
        long rem = side == C.BUY
                ? matchBuy(id, symbol, price, qty)
                : matchSell(id, symbol, price, qty);
        if (rem > 0 && tif == C.TIF_GTC) {
            rest(id, symbol, side, price, rem);
        } else if (rem > 0) {
            iocKilled += rem;
        }
    }

    public void market(int id, int symbol, int side, long qty) {
        long rem = side == C.BUY
                ? matchBuy(id, symbol, C.PRICE_MAX, qty)
                : matchSell(id, symbol, C.PRICE_MIN, qty);
        if (rem > 0) {
            iocKilled += rem;
        }
    }

    public void cancel(int id) {
        if (id < 0 || id >= orders.length) {
            cancelMisses++;
            return;
        }
        Order o = orders[id];
        if (o.qty <= 0) {
            cancelMisses++;
            return;
        }
        Book b = books[o.symbol];
        Level lvl = o.side == C.BUY ? b.bids[o.price] : b.asks[o.price];
        lvl.totalQty -= o.qty;
        o.qty = 0;
        cancelHits++;
        if (lvl.totalQty <= 0) {
            if (o.side == C.BUY && b.bestBid == o.price) {
                while (b.bestBid >= C.PRICE_MIN && b.bids[b.bestBid].totalQty <= 0) {
                    b.bestBid--;
                }
            } else if (o.side == C.SELL && b.bestAsk == o.price) {
                while (b.bestAsk <= C.PRICE_MAX && b.asks[b.bestAsk].totalQty <= 0) {
                    b.bestAsk++;
                }
            }
        }
    }

    public void finish() {
        for (Book b : books) {
            long bid = 0;
            long ask = 0;
            for (int p = C.PRICE_MIN; p <= C.PRICE_MAX; p++) {
                bid += b.bids[p].totalQty;
                ask += b.asks[p].totalQty;
            }
            checksum = checksum * C.MIX + bid;
            checksum = checksum * C.MIX + ask;
            checksum = checksum * C.MIX + fillCount;
            checksum = checksum * C.MIX + cancelHits;
        }
    }

    private void rest(int id, int symbol, int side, int price, long qty) {
        Order o = orders[id];
        o.symbol = symbol;
        o.side = side;
        o.price = price;
        o.qty = qty;
        Book b = books[symbol];
        if (side == C.BUY) {
            b.bids[price].add(id);
            b.bids[price].totalQty += qty;
            if (price > b.bestBid) {
                b.bestBid = price;
            }
        } else {
            b.asks[price].add(id);
            b.asks[price].totalQty += qty;
            if (price < b.bestAsk) {
                b.bestAsk = price;
            }
        }
        restCount++;
    }

    private long matchBuy(int takerId, int symbol, int limitPx, long qty) {
        Book b = books[symbol];
        while (qty > 0 && b.bestAsk <= C.PRICE_MAX && b.bestAsk <= limitPx) {
            qty = hitLevel(b.asks[b.bestAsk], takerId, b.bestAsk, qty);
            if (b.asks[b.bestAsk].totalQty <= 0) {
                b.bestAsk++;
                while (b.bestAsk <= C.PRICE_MAX && b.asks[b.bestAsk].totalQty <= 0) {
                    b.bestAsk++;
                }
            }
        }
        return qty;
    }

    private long matchSell(int takerId, int symbol, int limitPx, long qty) {
        Book b = books[symbol];
        while (qty > 0 && b.bestBid >= C.PRICE_MIN && b.bestBid >= limitPx) {
            qty = hitLevel(b.bids[b.bestBid], takerId, b.bestBid, qty);
            if (b.bids[b.bestBid].totalQty <= 0) {
                b.bestBid--;
                while (b.bestBid >= C.PRICE_MIN && b.bids[b.bestBid].totalQty <= 0) {
                    b.bestBid--;
                }
            }
        }
        return qty;
    }

    private long hitLevel(Level lvl, int takerId, int px, long qty) {
        for (int i = 0; i < lvl.size && qty > 0; i++) {
            int mid = lvl.ids[i];
            Order m = orders[mid];
            if (m.qty <= 0) {
                continue;
            }
            long fill = Math.min(qty, m.qty);
            m.qty -= fill;
            qty -= fill;
            lvl.totalQty -= fill;
            fillCount++;
            fillQty += fill;
            checksum = checksum * C.MIX + takerId;
            checksum = checksum * C.MIX + mid;
            checksum = checksum * C.MIX + px;
            checksum = checksum * C.MIX + fill;
        }
        return qty;
    }
}
