from .. import constants as c
from ..rng import XorShift64
from .agg import TickAgg


class MarketDataBench:
    def __init__(self, seed: int, n_ticks: int, n_symbols: int) -> None:
        self.symbol = [0] * n_ticks
        self.price = [0] * n_ticks
        self.qty = [0] * n_ticks
        rng = XorShift64(seed)
        for i in range(n_ticks):
            self.symbol[i] = rng.next_bounded(n_symbols)
            self.price[i] = c.PRICE_MID + rng.next_bounded(c.PRICE_SPAN) - c.PRICE_SPAN // 2
            self.qty[i] = 1 + rng.next_bounded(100)
        self.aggs = [TickAgg() for _ in range(n_symbols)]
        self.checksum = 0

    def apply(self, i: int) -> None:
        self.aggs[self.symbol[i]].on_tick(self.price[i], self.qty[i])

    def reset_aggs(self) -> None:
        for a in self.aggs:
            a.reset()
        self.checksum = 0

    def finish(self) -> None:
        checksum = self.checksum
        for a in self.aggs:
            checksum = a.mix(checksum)
        self.checksum = checksum
