package com.equities.bench;

import java.nio.charset.StandardCharsets;
import java.util.Locale;

public final class FixBench {
    public final byte[] arena;
    public final int[] off;
    public final int[] len;
    public final int nMsgs;
    public long checksum;

    public FixBench(long seed, int nMsgs, int nSymbols) {
        this.nMsgs = nMsgs;
        this.off = new int[nMsgs];
        this.len = new int[nMsgs];
        byte[][] tmp = new byte[nMsgs][];
        XorShift64 rng = new XorShift64(seed);
        int total = 0;
        for (int i = 0; i < nMsgs; i++) {
            long clOrdId = i + 1L;
            int symbol = rng.nextBounded(nSymbols);
            int side = rng.nextBounded(2);
            long qty = 1 + rng.nextBounded(100);
            int price = C.PRICE_MID + rng.nextBounded(C.PRICE_SPAN) - C.PRICE_SPAN / 2;
            tmp[i] = buildNewOrderSingle(i + 1, clOrdId, symbol, side, qty, price);
            total += tmp[i].length;
        }
        this.arena = new byte[total];
        int cursor = 0;
        for (int i = 0; i < nMsgs; i++) {
            off[i] = cursor;
            len[i] = tmp[i].length;
            System.arraycopy(tmp[i], 0, arena, cursor, tmp[i].length);
            cursor += tmp[i].length;
        }
    }

    public void parse(int i) {
        int start = off[i];
        int end = start + len[i];
        long clOrdId = 0;
        int symbol = 0;
        int side = 0;
        long qty = 0;
        long price = 0;
        int p = start;
        while (p < end) {
            int tag = 0;
            while (p < end && arena[p] != '=') {
                byte c = arena[p];
                if (c >= '0' && c <= '9') {
                    tag = tag * 10 + (c - '0');
                }
                p++;
            }
            if (p < end && arena[p] == '=') {
                p++;
            }
            int vs = p;
            while (p < end && arena[p] != 1) {
                p++;
            }
            int ve = p;
            if (p < end && arena[p] == 1) {
                p++;
            }
            switch (tag) {
                case 11 -> clOrdId = parseLong(arena, vs, ve);
                case 55 -> symbol = parseSymbol(arena, vs, ve);
                case 54 -> {
                    long v = parseLong(arena, vs, ve);
                    side = v == 1 ? C.BUY : C.SELL;
                }
                case 38 -> qty = parseLong(arena, vs, ve);
                case 44 -> price = parseLong(arena, vs, ve);
                default -> {
                }
            }
        }
        checksum = checksum * C.MIX + clOrdId;
        checksum = checksum * C.MIX + symbol;
        checksum = checksum * C.MIX + side;
        checksum = checksum * C.MIX + qty;
        checksum = checksum * C.MIX + price;
    }

    static byte[] buildNewOrderSingle(int seq, long clOrdId, int symbol, int side, long qty, long price) {
        String body = "35=D\u0001"
                + "34=" + seq + "\u0001"
                + "49=SENDER\u0001"
                + "56=TARGET\u0001"
                + "11=" + clOrdId + "\u0001"
                + "55=SYM" + pad2(symbol) + "\u0001"
                + "54=" + (side == C.BUY ? "1" : "2") + "\u0001"
                + "38=" + qty + "\u0001"
                + "44=" + price + "\u0001"
                + "40=2\u0001"
                + "59=0\u0001";
        String header = "8=FIX.4.4\u00019=" + body.length() + "\u0001";
        String prefix = header + body;
        int sum = 0;
        for (int i = 0; i < prefix.length(); i++) {
            sum += prefix.charAt(i) & 0xff;
        }
        String msg = prefix + "10=" + pad3(sum % 256) + "\u0001";
        return msg.getBytes(StandardCharsets.US_ASCII);
    }

    static int parseSymbol(byte[] b, int s, int e) {
        int k = s;
        if (e - s >= 3 && b[k] == 'S' && b[k + 1] == 'Y' && b[k + 2] == 'M') {
            k += 3;
        }
        return (int) parseLong(b, k, e);
    }

    static long parseLong(byte[] b, int s, int e) {
        long v = 0;
        for (int i = s; i < e; i++) {
            byte c = b[i];
            if (c >= '0' && c <= '9') {
                v = v * 10 + (c - '0');
            }
        }
        return v;
    }

    static String pad2(int n) {
        return n < 10 ? "0" + n : Integer.toString(n);
    }

    static String pad3(int n) {
        return String.format(Locale.ROOT, "%03d", n);
    }
}
