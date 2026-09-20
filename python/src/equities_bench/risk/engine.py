from .. import constants as c
from ..workload import Workload


class RiskEngine:
    def __init__(self, n_accounts: int, n_symbols: int) -> None:
        self.position = [[0] * n_symbols for _ in range(n_accounts)]
        self.acct_notional = [0] * n_accounts
        self.last_px = [c.PRICE_MID] * n_symbols
        self.accepts = 0
        self.rejects = 0
        self.checksum = 0

    def apply(self, w: Workload, i: int) -> None:
        if w.op[i] == c.OP_CANCEL:
            return
        acct = w.account[i]
        sym = w.symbol[i]
        side = w.side[i]
        qty = w.qty[i]
        px = self.last_px[sym] if w.price[i] == 0 else w.price[i]
        signed_qty = qty if side == c.BUY else -qty
        notional = px * qty

        reject = qty <= 0 or qty > c.MAX_ORDER_QTY
        if not reject:
            abs_pos = abs(self.position[acct][sym] + signed_qty)
            if abs_pos > c.MAX_POSITION:
                reject = True
            elif self.acct_notional[acct] + notional > c.MAX_NOTIONAL:
                reject = True
            elif self.last_px[sym] > 0 and abs(px - self.last_px[sym]) * 10_000 > self.last_px[sym] * c.COLLAR_BPS:
                reject = True
        if reject:
            self.rejects += 1
        else:
            self.position[acct][sym] += signed_qty
            self.acct_notional[acct] += notional
            self.last_px[sym] = px
            self.accepts += 1

    def finish(self) -> None:
        self.checksum = c.mix(self.checksum, self.accepts)
        self.checksum = c.mix(self.checksum, self.rejects)
        for row in self.position:
            for p in row:
                self.checksum = c.mix_i64(self.checksum, p)
