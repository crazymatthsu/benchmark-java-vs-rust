from .. import constants as c
from ..rng import XorShift64


def pad2(n: int) -> str:
    return f"0{n}" if n < 10 else str(n)


def pad3(n: int) -> str:
    return f"{n:03d}"


def build_new_order_single(seq: int, cl_ord_id: int, symbol: int, side: int, qty: int, price: int) -> bytes:
    body = (
        "35=D\x01"
        f"34={seq}\x01"
        "49=SENDER\x01"
        "56=TARGET\x01"
        f"11={cl_ord_id}\x01"
        f"55=SYM{pad2(symbol)}\x01"
        f"54={1 if side == c.BUY else 2}\x01"
        f"38={qty}\x01"
        f"44={price}\x01"
        "40=2\x01"
        "59=0\x01"
    )
    header = f"8=FIX.4.4\x019={len(body)}\x01"
    prefix = header + body
    checksum = sum(prefix.encode("ascii")) % 256
    return (prefix + f"10={pad3(checksum)}\x01").encode("ascii")


def parse_long(buf: bytes, s: int, e: int) -> int:
    v = 0
    for i in range(s, e):
        ch = buf[i]
        if 48 <= ch <= 57:
            v = v * 10 + (ch - 48)
    return v


def parse_symbol(buf: bytes, s: int, e: int) -> int:
    k = s
    if e - s >= 3 and buf[k] == 83 and buf[k + 1] == 89 and buf[k + 2] == 77:
        k += 3
    return parse_long(buf, k, e)


class FixBench:
    def __init__(self, seed: int, n_msgs: int, n_symbols: int) -> None:
        tmp = []
        rng = XorShift64(seed)
        total = 0
        for i in range(n_msgs):
            cl_ord_id = i + 1
            symbol = rng.next_bounded(n_symbols)
            side = rng.next_bounded(2)
            qty = 1 + rng.next_bounded(100)
            price = c.PRICE_MID + rng.next_bounded(c.PRICE_SPAN) - c.PRICE_SPAN // 2
            msg = build_new_order_single(i + 1, cl_ord_id, symbol, side, qty, price)
            total += len(msg)
            tmp.append(msg)
        self.arena = bytearray(total)
        self.off = [0] * n_msgs
        self.len = [0] * n_msgs
        cursor = 0
        for i, msg in enumerate(tmp):
            self.off[i] = cursor
            self.len[i] = len(msg)
            self.arena[cursor : cursor + len(msg)] = msg
            cursor += len(msg)
        self.checksum = 0

    def parse(self, i: int) -> None:
        start = self.off[i]
        end = start + self.len[i]
        arena = self.arena
        cl_ord_id = 0
        symbol = 0
        side = 0
        qty = 0
        price = 0
        p = start
        while p < end:
            tag = 0
            while p < end and arena[p] != 61:
                ch = arena[p]
                if 48 <= ch <= 57:
                    tag = tag * 10 + (ch - 48)
                p += 1
            if p < end and arena[p] == 61:
                p += 1
            vs = p
            while p < end and arena[p] != 1:
                p += 1
            ve = p
            if p < end and arena[p] == 1:
                p += 1
            if tag == 11:
                cl_ord_id = parse_long(arena, vs, ve)
            elif tag == 55:
                symbol = parse_symbol(arena, vs, ve)
            elif tag == 54:
                v = parse_long(arena, vs, ve)
                side = c.BUY if v == 1 else c.SELL
            elif tag == 38:
                qty = parse_long(arena, vs, ve)
            elif tag == 44:
                price = parse_long(arena, vs, ve)
        self.checksum = c.mix_i64(self.checksum, cl_ord_id)
        self.checksum = c.mix_i32(self.checksum, symbol)
        self.checksum = c.mix_i32(self.checksum, side)
        self.checksum = c.mix_i64(self.checksum, qty)
        self.checksum = c.mix_i64(self.checksum, price)
