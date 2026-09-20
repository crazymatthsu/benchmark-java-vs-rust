package com.equities.bench;

public final class C {
    private C() {}

    public static final int PRICE_MIN = 1;
    public static final int PRICE_MAX = 20_000;
    public static final int PRICE_MID = 10_000;
    public static final int PRICE_SPAN = 200;
    public static final long MIX = 1_000_003L;
    public static final long DEFAULT_SEED = 0x0C0FFEE123456789L;

    public static final byte BUY = 0;
    public static final byte SELL = 1;
    public static final byte TIF_GTC = 0;
    public static final byte TIF_IOC = 1;
    public static final byte OP_ADD = 0;
    public static final byte OP_CANCEL = 1;
    public static final byte OP_MARKET = 2;

    public static final long MAX_ORDER_QTY = 10_000;
    public static final long MAX_POSITION = 50_000;
    public static final long MAX_NOTIONAL = 1_000_000_000_000L;
    public static final long COLLAR_BPS = 200;
}
