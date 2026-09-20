from . import constants as c


class XorShift64:
    def __init__(self, seed: int) -> None:
        self.state = c.DEFAULT_SEED if seed == 0 else seed & c.U64

    def next_u64(self) -> int:
        x = self.state
        x ^= (x << 13) & c.U64
        x ^= x >> 7
        x ^= (x << 17) & c.U64
        x &= c.U64
        self.state = x
        return x

    def next_bounded(self, n: int) -> int:
        return self.next_u64() % n
