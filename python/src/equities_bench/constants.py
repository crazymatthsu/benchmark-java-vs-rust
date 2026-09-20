PRICE_MIN = 1
PRICE_MAX = 20_000
PRICE_MID = 10_000
PRICE_SPAN = 200
MIX = 1_000_003
DEFAULT_SEED = 0x0C0FFEE123456789
U64 = 0xFFFFFFFFFFFFFFFF

BUY = 0
SELL = 1
TIF_GTC = 0
TIF_IOC = 1
OP_ADD = 0
OP_CANCEL = 1
OP_MARKET = 2

MAX_ORDER_QTY = 10_000
MAX_POSITION = 50_000
MAX_NOTIONAL = 1_000_000_000_000
COLLAR_BPS = 200

I64_MIN = -9223372036854775808
I64_MAX = 9223372036854775807


def mix(checksum: int, x: int) -> int:
    return (checksum * MIX + (x & U64)) & U64


def mix_i64(checksum: int, x: int) -> int:
    return mix(checksum, x)


def mix_i32(checksum: int, x: int) -> int:
    return mix(checksum, x)
