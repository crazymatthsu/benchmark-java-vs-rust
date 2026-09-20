#pragma once

#include "../constants.hpp"
#include "../workload.hpp"

#include <cstdint>
#include <vector>

struct Order {
    int32_t symbol = 0;
    int32_t side = 0;
    int32_t price = 0;
    int64_t qty = 0;
};

struct Level {
    std::vector<int32_t> ids;
    int64_t total_qty = 0;

    Level() { ids.reserve(8); }

    void add(int32_t id) { ids.push_back(id); }
};

struct Book {
    std::vector<Level> bids;
    std::vector<Level> asks;
    int32_t best_bid = 0;
    int32_t best_ask = c::PRICE_MAX + 1;

    Book() {
        bids.resize(static_cast<std::size_t>(c::PRICE_MAX) + 1);
        asks.resize(static_cast<std::size_t>(c::PRICE_MAX) + 1);
        for (auto& lvl : bids) {
            lvl.ids.reserve(8);
        }
        for (auto& lvl : asks) {
            lvl.ids.reserve(8);
        }
    }
};

struct OrderBookEngine {
    std::vector<Order> orders;
    std::vector<Book> books;
    uint64_t checksum = 0;
    uint64_t fill_count = 0;
    int64_t fill_qty = 0;
    uint64_t cancel_hits = 0;
    uint64_t cancel_misses = 0;
    int64_t ioc_killed = 0;
    uint64_t rest_count = 0;

    OrderBookEngine(int32_t n_symbols, int32_t max_order_id)
        : orders(static_cast<std::size_t>(max_order_id) + 1),
          books(static_cast<std::size_t>(n_symbols)) {}

    void apply(const Workload& w, std::size_t i) {
        switch (w.op[i]) {
            case c::OP_ADD:
                add(w.order_id[i], w.symbol[i], w.side[i], w.price[i], w.qty[i], w.tif[i]);
                break;
            case c::OP_CANCEL:
                cancel(w.order_id[i]);
                break;
            default:
                market(w.order_id[i], w.symbol[i], w.side[i], w.qty[i]);
                break;
        }
    }

    void add(int32_t id, int32_t symbol, int32_t side, int32_t price, int64_t qty, uint8_t tif) {
        int64_t rem = side == c::BUY ? match_buy(id, symbol, price, qty)
                                     : match_sell(id, symbol, price, qty);
        if (rem > 0 && tif == c::TIF_GTC) {
            rest(id, symbol, side, price, rem);
        } else if (rem > 0) {
            ioc_killed += rem;
        }
    }

    void market(int32_t id, int32_t symbol, int32_t side, int64_t qty) {
        int64_t rem = side == c::BUY ? match_buy(id, symbol, c::PRICE_MAX, qty)
                                     : match_sell(id, symbol, c::PRICE_MIN, qty);
        if (rem > 0) {
            ioc_killed += rem;
        }
    }

    void cancel(int32_t id) {
        if (id < 0 || static_cast<std::size_t>(id) >= orders.size()) {
            cancel_misses++;
            return;
        }
        Order& o = orders[static_cast<std::size_t>(id)];
        if (o.qty <= 0) {
            cancel_misses++;
            return;
        }
        Book& book = books[static_cast<std::size_t>(o.symbol)];
        Level& lvl = o.side == c::BUY ? book.bids[static_cast<std::size_t>(o.price)]
                                      : book.asks[static_cast<std::size_t>(o.price)];
        lvl.total_qty -= o.qty;
        o.qty = 0;
        cancel_hits++;
        if (lvl.total_qty <= 0) {
            if (o.side == c::BUY && book.best_bid == o.price) {
                while (book.best_bid >= c::PRICE_MIN
                       && book.bids[static_cast<std::size_t>(book.best_bid)].total_qty <= 0) {
                    book.best_bid--;
                }
            } else if (o.side == c::SELL && book.best_ask == o.price) {
                while (book.best_ask <= c::PRICE_MAX
                       && book.asks[static_cast<std::size_t>(book.best_ask)].total_qty <= 0) {
                    book.best_ask++;
                }
            }
        }
    }

    void finish() {
        for (const Book& book : books) {
            int64_t bid = 0;
            int64_t ask = 0;
            for (int32_t p = c::PRICE_MIN; p <= c::PRICE_MAX; p++) {
                bid += book.bids[static_cast<std::size_t>(p)].total_qty;
                ask += book.asks[static_cast<std::size_t>(p)].total_qty;
            }
            checksum = c::mix_i64(checksum, bid);
            checksum = c::mix_i64(checksum, ask);
            checksum = c::mix(checksum, fill_count);
            checksum = c::mix(checksum, cancel_hits);
        }
    }

private:
    void rest(int32_t id, int32_t symbol, int32_t side, int32_t price, int64_t qty) {
        Order& o = orders[static_cast<std::size_t>(id)];
        o.symbol = symbol;
        o.side = side;
        o.price = price;
        o.qty = qty;
        Book& book = books[static_cast<std::size_t>(symbol)];
        if (side == c::BUY) {
            book.bids[static_cast<std::size_t>(price)].add(id);
            book.bids[static_cast<std::size_t>(price)].total_qty += qty;
            if (price > book.best_bid) {
                book.best_bid = price;
            }
        } else {
            book.asks[static_cast<std::size_t>(price)].add(id);
            book.asks[static_cast<std::size_t>(price)].total_qty += qty;
            if (price < book.best_ask) {
                book.best_ask = price;
            }
        }
        rest_count++;
    }

    int64_t match_buy(int32_t taker_id, int32_t symbol, int32_t limit_px, int64_t qty) {
        Book& book = books[static_cast<std::size_t>(symbol)];
        while (qty > 0 && book.best_ask <= c::PRICE_MAX && book.best_ask <= limit_px) {
            qty = hit_level(book.asks[static_cast<std::size_t>(book.best_ask)], taker_id, book.best_ask, qty);
            if (book.asks[static_cast<std::size_t>(book.best_ask)].total_qty <= 0) {
                book.best_ask++;
                while (book.best_ask <= c::PRICE_MAX
                       && book.asks[static_cast<std::size_t>(book.best_ask)].total_qty <= 0) {
                    book.best_ask++;
                }
            }
        }
        return qty;
    }

    int64_t match_sell(int32_t taker_id, int32_t symbol, int32_t limit_px, int64_t qty) {
        Book& book = books[static_cast<std::size_t>(symbol)];
        while (qty > 0 && book.best_bid >= c::PRICE_MIN && book.best_bid >= limit_px) {
            qty = hit_level(book.bids[static_cast<std::size_t>(book.best_bid)], taker_id, book.best_bid, qty);
            if (book.bids[static_cast<std::size_t>(book.best_bid)].total_qty <= 0) {
                book.best_bid--;
                while (book.best_bid >= c::PRICE_MIN
                       && book.bids[static_cast<std::size_t>(book.best_bid)].total_qty <= 0) {
                    book.best_bid--;
                }
            }
        }
        return qty;
    }

    int64_t hit_level(Level& lvl, int32_t taker_id, int32_t px, int64_t qty) {
        for (std::size_t i = 0; i < lvl.ids.size() && qty > 0; i++) {
            int32_t mid = lvl.ids[i];
            Order& m = orders[static_cast<std::size_t>(mid)];
            if (m.qty <= 0) {
                continue;
            }
            int64_t fill = qty < m.qty ? qty : m.qty;
            m.qty -= fill;
            qty -= fill;
            lvl.total_qty -= fill;
            fill_count++;
            fill_qty += fill;
            checksum = c::mix_i32(checksum, taker_id);
            checksum = c::mix_i32(checksum, mid);
            checksum = c::mix_i32(checksum, px);
            checksum = c::mix_i64(checksum, fill);
        }
        return qty;
    }
};
