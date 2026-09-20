from .. import constants as c
from ..rng import XorShift64
from .agg import TickAgg

REC = 32


def write_u32(a: bytearray, o: int, v: int) -> None:
    a[o] = v & 0xFF
    a[o + 1] = (v >> 8) & 0xFF
    a[o + 2] = (v >> 16) & 0xFF
    a[o + 3] = (v >> 24) & 0xFF


def write_i64(a: bytearray, o: int, v: int) -> None:
    u = v & c.U64
    a[o] = u & 0xFF
    a[o + 1] = (u >> 8) & 0xFF
    a[o + 2] = (u >> 16) & 0xFF
    a[o + 3] = (u >> 24) & 0xFF
    a[o + 4] = (u >> 32) & 0xFF
    a[o + 5] = (u >> 40) & 0xFF
    a[o + 6] = (u >> 48) & 0xFF
    a[o + 7] = (u >> 56) & 0xFF


def read_u32(a: bytearray, o: int) -> int:
    return a[o] | (a[o + 1] << 8) | (a[o + 2] << 16) | (a[o + 3] << 24)


def read_i64(a: bytearray, o: int) -> int:
    u = (
        a[o]
        | (a[o + 1] << 8)
        | (a[o + 2] << 16)
        | (a[o + 3] << 24)
        | (a[o + 4] << 32)
        | (a[o + 5] << 40)
        | (a[o + 6] << 48)
        | (a[o + 7] << 56)
    )
    if u >= 2**63:
        return u - 2**64
    return u


class BinaryMdBench:
    def __init__(self, seed: int, n_ticks: int, n_symbols: int) -> None:
        self.arena = bytearray(n_ticks * REC)
        rng = XorShift64(seed)
        o = 0
        for i in range(n_ticks):
            symbol = rng.next_bounded(n_symbols)
            price = c.PRICE_MID + rng.next_bounded(c.PRICE_SPAN) - c.PRICE_SPAN // 2
            qty = 1 + rng.next_bounded(100)
            write_u32(self.arena, o, symbol)
            write_u32(self.arena, o + 4, 0)
            write_i64(self.arena, o + 8, price)
            write_i64(self.arena, o + 16, qty)
            write_i64(self.arena, o + 24, i)
            o += REC
        self.aggs = [TickAgg() for _ in range(n_symbols)]
        self.checksum = 0

    def apply(self, i: int) -> None:
        o = i * REC
        symbol = read_u32(self.arena, o)
        price = read_i64(self.arena, o + 8)
        qty = read_i64(self.arena, o + 16)
        self.aggs[symbol].on_tick(price, qty)

    def reset_aggs(self) -> None:
        for a in self.aggs:
            a.reset()
        self.checksum = 0

    def finish(self) -> None:
        checksum = self.checksum
        for a in self.aggs:
            checksum = a.mix(checksum)
        self.checksum = checksum
