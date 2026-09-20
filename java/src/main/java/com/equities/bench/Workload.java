package com.equities.bench;

public final class Workload {
    public final byte[] op;
    public final int[] orderId;
    public final int[] symbol;
    public final byte[] side;
    public final int[] price;
    public final long[] qty;
    public final byte[] tif;
    public final int[] account;
    public final int nOps;
    public final int nSymbols;
    public final int nAccounts;

    private Workload(int nOps, int nSymbols, int nAccounts) {
        this.nOps = nOps;
        this.nSymbols = nSymbols;
        this.nAccounts = nAccounts;
        this.op = new byte[nOps];
        this.orderId = new int[nOps];
        this.symbol = new int[nOps];
        this.side = new byte[nOps];
        this.price = new int[nOps];
        this.qty = new long[nOps];
        this.tif = new byte[nOps];
        this.account = new int[nOps];
    }

    public static Workload generate(long seed, int nOps, int nSymbols, int nAccounts) {
        Workload w = new Workload(nOps, nSymbols, nAccounts);
        XorShift64 rng = new XorShift64(seed);
        int nextId = 1;
        for (int i = 0; i < nOps; i++) {
            int r = rng.nextBounded(100);
            if (r < 70 || nextId == 1) {
                w.op[i] = C.OP_ADD;
                w.orderId[i] = nextId++;
                w.symbol[i] = rng.nextBounded(nSymbols);
                w.side[i] = (byte) rng.nextBounded(2);
                w.price[i] = C.PRICE_MID + rng.nextBounded(C.PRICE_SPAN) - C.PRICE_SPAN / 2;
                w.qty[i] = 1 + rng.nextBounded(100);
                w.tif[i] = rng.nextBounded(10) == 0 ? C.TIF_IOC : C.TIF_GTC;
                w.account[i] = rng.nextBounded(nAccounts);
            } else if (r < 90) {
                w.op[i] = C.OP_CANCEL;
                w.orderId[i] = 1 + rng.nextBounded(nextId - 1);
                w.account[i] = rng.nextBounded(nAccounts);
            } else {
                w.op[i] = C.OP_MARKET;
                w.orderId[i] = nextId++;
                w.symbol[i] = rng.nextBounded(nSymbols);
                w.side[i] = (byte) rng.nextBounded(2);
                w.price[i] = 0;
                w.qty[i] = 1 + rng.nextBounded(100);
                w.tif[i] = C.TIF_IOC;
                w.account[i] = rng.nextBounded(nAccounts);
            }
        }
        return w;
    }
}
