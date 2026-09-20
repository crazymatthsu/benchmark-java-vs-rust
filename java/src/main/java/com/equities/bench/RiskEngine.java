package com.equities.bench;

import java.util.Arrays;

public final class RiskEngine {
    public final long[][] position;
    public final long[] acctNotional;
    public final long[] lastPx;
    public long accepts;
    public long rejects;
    public long checksum;

    public RiskEngine(int nAccounts, int nSymbols) {
        position = new long[nAccounts][nSymbols];
        acctNotional = new long[nAccounts];
        lastPx = new long[nSymbols];
        Arrays.fill(lastPx, C.PRICE_MID);
    }

    public void apply(Workload w, int i) {
        if (w.op[i] == C.OP_CANCEL) {
            return;
        }
        int acct = w.account[i];
        int sym = w.symbol[i];
        int side = w.side[i];
        long qty = w.qty[i];
        long px = w.price[i] == 0 ? lastPx[sym] : w.price[i];
        long signedQty = side == C.BUY ? qty : -qty;
        long notional = px * qty;

        boolean reject = qty <= 0 || qty > C.MAX_ORDER_QTY;
        if (!reject) {
            long absPos = Math.abs(position[acct][sym] + signedQty);
            if (absPos > C.MAX_POSITION) {
                reject = true;
            } else if (acctNotional[acct] + notional > C.MAX_NOTIONAL) {
                reject = true;
            } else if (lastPx[sym] > 0
                    && Math.abs(px - lastPx[sym]) * 10_000L > lastPx[sym] * C.COLLAR_BPS) {
                reject = true;
            }
        }
        if (reject) {
            rejects++;
        } else {
            position[acct][sym] += signedQty;
            acctNotional[acct] += notional;
            lastPx[sym] = px;
            accepts++;
        }
    }

    public void finish() {
        checksum = checksum * C.MIX + accepts;
        checksum = checksum * C.MIX + rejects;
        for (long[] row : position) {
            for (long p : row) {
                checksum = checksum * C.MIX + p;
            }
        }
    }
}
