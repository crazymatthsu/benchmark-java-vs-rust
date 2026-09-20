from . import constants as c
from .rng import XorShift64


class Workload:
    def __init__(self, n_ops: int) -> None:
        self.op = [0] * n_ops
        self.order_id = [0] * n_ops
        self.symbol = [0] * n_ops
        self.side = [0] * n_ops
        self.price = [0] * n_ops
        self.qty = [0] * n_ops
        self.tif = [0] * n_ops
        self.account = [0] * n_ops
        self.n_ops = n_ops

    @staticmethod
    def generate(seed: int, n_ops: int, n_symbols: int, n_accounts: int) -> "Workload":
        w = Workload(n_ops)
        rng = XorShift64(seed)
        next_id = 1
        for i in range(n_ops):
            r = rng.next_bounded(100)
            if r < 70 or next_id == 1:
                w.op[i] = c.OP_ADD
                w.order_id[i] = next_id
                next_id += 1
                w.symbol[i] = rng.next_bounded(n_symbols)
                w.side[i] = rng.next_bounded(2)
                w.price[i] = c.PRICE_MID + rng.next_bounded(c.PRICE_SPAN) - c.PRICE_SPAN // 2
                w.qty[i] = 1 + rng.next_bounded(100)
                w.tif[i] = c.TIF_IOC if rng.next_bounded(10) == 0 else c.TIF_GTC
                w.account[i] = rng.next_bounded(n_accounts)
            elif r < 90:
                w.op[i] = c.OP_CANCEL
                w.order_id[i] = 1 + rng.next_bounded(next_id - 1)
                w.account[i] = rng.next_bounded(n_accounts)
            else:
                w.op[i] = c.OP_MARKET
                w.order_id[i] = next_id
                next_id += 1
                w.symbol[i] = rng.next_bounded(n_symbols)
                w.side[i] = rng.next_bounded(2)
                w.price[i] = 0
                w.qty[i] = 1 + rng.next_bounded(100)
                w.tif[i] = c.TIF_IOC
                w.account[i] = rng.next_bounded(n_accounts)
        return w
