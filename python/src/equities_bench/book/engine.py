from .. import constants as c
from ..workload import Workload


class Order:
    __slots__ = ("symbol", "side", "price", "qty")

    def __init__(self) -> None:
        self.symbol = 0
        self.side = 0
        self.price = 0
        self.qty = 0


class Level:
    __slots__ = ("ids", "total_qty")

    def __init__(self) -> None:
        self.ids: list[int] = []
        self.total_qty = 0

    def add(self, oid: int) -> None:
        self.ids.append(oid)


class Book:
    def __init__(self) -> None:
        n = c.PRICE_MAX + 1
        self.bids = [Level() for _ in range(n)]
        self.asks = [Level() for _ in range(n)]
        self.best_bid = 0
        self.best_ask = c.PRICE_MAX + 1


class OrderBookEngine:
    def __init__(self, n_symbols: int, max_order_id: int) -> None:
        self.books = [Book() for _ in range(n_symbols)]
        self.orders = [Order() for _ in range(max_order_id + 1)]
        self.checksum = 0
        self.fill_count = 0
        self.fill_qty = 0
        self.cancel_hits = 0
        self.cancel_misses = 0
        self.ioc_killed = 0
        self.rest_count = 0

    def apply(self, w: Workload, i: int) -> None:
        op = w.op[i]
        if op == c.OP_ADD:
            self.add(w.order_id[i], w.symbol[i], w.side[i], w.price[i], w.qty[i], w.tif[i])
        elif op == c.OP_CANCEL:
            self.cancel(w.order_id[i])
        else:
            self.market(w.order_id[i], w.symbol[i], w.side[i], w.qty[i])

    def add(self, oid: int, symbol: int, side: int, price: int, qty: int, tif: int) -> None:
        rem = self.match_buy(oid, symbol, price, qty) if side == c.BUY else self.match_sell(
            oid, symbol, price, qty
        )
        if rem > 0 and tif == c.TIF_GTC:
            self.rest(oid, symbol, side, price, rem)
        elif rem > 0:
            self.ioc_killed += rem

    def market(self, oid: int, symbol: int, side: int, qty: int) -> None:
        rem = (
            self.match_buy(oid, symbol, c.PRICE_MAX, qty)
            if side == c.BUY
            else self.match_sell(oid, symbol, c.PRICE_MIN, qty)
        )
        if rem > 0:
            self.ioc_killed += rem

    def cancel(self, oid: int) -> None:
        if oid < 0 or oid >= len(self.orders):
            self.cancel_misses += 1
            return
        o = self.orders[oid]
        if o.qty <= 0:
            self.cancel_misses += 1
            return
        book = self.books[o.symbol]
        lvl = book.bids[o.price] if o.side == c.BUY else book.asks[o.price]
        lvl.total_qty -= o.qty
        o.qty = 0
        self.cancel_hits += 1
        if lvl.total_qty <= 0:
            if o.side == c.BUY and book.best_bid == o.price:
                while book.best_bid >= c.PRICE_MIN and book.bids[book.best_bid].total_qty <= 0:
                    book.best_bid -= 1
            elif o.side == c.SELL and book.best_ask == o.price:
                while book.best_ask <= c.PRICE_MAX and book.asks[book.best_ask].total_qty <= 0:
                    book.best_ask += 1

    def finish(self) -> None:
        for book in self.books:
            bid = 0
            ask = 0
            for p in range(c.PRICE_MIN, c.PRICE_MAX + 1):
                bid += book.bids[p].total_qty
                ask += book.asks[p].total_qty
            self.checksum = c.mix_i64(self.checksum, bid)
            self.checksum = c.mix_i64(self.checksum, ask)
            self.checksum = c.mix(self.checksum, self.fill_count)
            self.checksum = c.mix(self.checksum, self.cancel_hits)

    def rest(self, oid: int, symbol: int, side: int, price: int, qty: int) -> None:
        o = self.orders[oid]
        o.symbol = symbol
        o.side = side
        o.price = price
        o.qty = qty
        book = self.books[symbol]
        if side == c.BUY:
            book.bids[price].add(oid)
            book.bids[price].total_qty += qty
            if price > book.best_bid:
                book.best_bid = price
        else:
            book.asks[price].add(oid)
            book.asks[price].total_qty += qty
            if price < book.best_ask:
                book.best_ask = price
        self.rest_count += 1

    def match_buy(self, taker_id: int, symbol: int, limit_px: int, qty: int) -> int:
        book = self.books[symbol]
        while qty > 0 and book.best_ask <= c.PRICE_MAX and book.best_ask <= limit_px:
            qty = self.hit_level(book.asks[book.best_ask], taker_id, book.best_ask, qty)
            if book.asks[book.best_ask].total_qty <= 0:
                book.best_ask += 1
                while book.best_ask <= c.PRICE_MAX and book.asks[book.best_ask].total_qty <= 0:
                    book.best_ask += 1
        return qty

    def match_sell(self, taker_id: int, symbol: int, limit_px: int, qty: int) -> int:
        book = self.books[symbol]
        while qty > 0 and book.best_bid >= c.PRICE_MIN and book.best_bid >= limit_px:
            qty = self.hit_level(book.bids[book.best_bid], taker_id, book.best_bid, qty)
            if book.bids[book.best_bid].total_qty <= 0:
                book.best_bid -= 1
                while book.best_bid >= c.PRICE_MIN and book.bids[book.best_bid].total_qty <= 0:
                    book.best_bid -= 1
        return qty

    def hit_level(self, lvl: Level, taker_id: int, px: int, qty: int) -> int:
        for mid in lvl.ids:
            if qty <= 0:
                break
            m = self.orders[mid]
            if m.qty <= 0:
                continue
            fill = qty if qty < m.qty else m.qty
            m.qty -= fill
            qty -= fill
            lvl.total_qty -= fill
            self.fill_count += 1
            self.fill_qty += fill
            self.checksum = c.mix_i32(self.checksum, taker_id)
            self.checksum = c.mix_i32(self.checksum, mid)
            self.checksum = c.mix_i32(self.checksum, px)
            self.checksum = c.mix_i64(self.checksum, fill)
        return qty
