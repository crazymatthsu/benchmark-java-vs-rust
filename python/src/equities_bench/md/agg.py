from .. import constants as c


class TickAgg:
    __slots__ = ("open", "high", "low", "close", "notional", "volume", "ticks", "bars")

    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self.open = 0
        self.high = c.I64_MIN
        self.low = c.I64_MAX
        self.close = 0
        self.notional = 0
        self.volume = 0
        self.ticks = 0
        self.bars = 0

    def on_tick(self, px: int, qty: int) -> None:
        if self.ticks == 0:
            self.open = px
        if px > self.high:
            self.high = px
        if px < self.low:
            self.low = px
        self.close = px
        self.notional += px * qty
        self.volume += qty
        self.ticks += 1
        if self.ticks % 100 == 0:
            self.bars += 1

    def mix(self, checksum: int) -> int:
        checksum = c.mix_i64(checksum, self.open)
        checksum = c.mix_i64(checksum, self.high)
        checksum = c.mix_i64(checksum, self.low)
        checksum = c.mix_i64(checksum, self.close)
        checksum = c.mix_i64(checksum, self.notional)
        checksum = c.mix_i64(checksum, self.volume)
        checksum = c.mix_i64(checksum, self.bars)
        return checksum
