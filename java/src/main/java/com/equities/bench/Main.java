package com.equities.bench;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Set;

public final class Main {
    public static void main(String[] args) throws Exception {
        Config cfg = Config.parse(args);
        if (cfg.help) {
            Config.printHelp();
            return;
        }
        System.out.printf(
                Locale.US,
                "java-bench ops=%d warmup=%d seed=%s symbols=%d accounts=%d benches=%s%n",
                cfg.ops,
                cfg.warmup,
                Long.toUnsignedString(cfg.seed),
                cfg.symbols,
                cfg.accounts,
                cfg.benches);
        EnvInfo env = new EnvInfo();
        List<BenchResult> results = new ArrayList<>();
        if (cfg.want("order-book")) {
            results.add(runOrderBook(cfg));
        }
        if (cfg.want("fix-parse")) {
            results.add(runFix(cfg));
        }
        if (cfg.want("risk")) {
            results.add(runRisk(cfg));
        }
        if (cfg.want("market-data")) {
            results.add(runMarketData(cfg));
        }
        if (cfg.want("binary-md")) {
            results.add(runBinaryMd(cfg));
        }
        JsonOut.write(
                cfg.output,
                env,
                cfg.runtimeName,
                cfg.seed,
                cfg.ops,
                cfg.warmup,
                cfg.symbols,
                cfg.accounts,
                results);
        System.out.println("wrote " + cfg.output.toAbsolutePath());
    }

    private static BenchResult runOrderBook(Config cfg) {
        log("generating order-book workload");
        Workload w = Workload.generate(cfg.seed, cfg.ops, cfg.symbols, cfg.accounts);
        int warmup = Math.min(cfg.warmup, cfg.ops);
        log("warming order-book (" + warmup + " ops)");
        OrderBookEngine throwaway = new OrderBookEngine(cfg.symbols, cfg.ops);
        for (int i = 0; i < warmup; i++) {
            throwaway.apply(w, i);
        }
        throwaway = null;
        System.gc();
        log("measuring order-book");
        OrderBookEngine eng = new OrderBookEngine(cfg.symbols, cfg.ops);
        long[] samples = new long[cfg.ops];
        long t0 = System.nanoTime();
        for (int i = 0; i < cfg.ops; i++) {
            long s = System.nanoTime();
            eng.apply(w, i);
            samples[i] = System.nanoTime() - s;
        }
        long elapsed = System.nanoTime() - t0;
        eng.finish();
        Percentiles.sortInPlace(samples);
        BenchResult r = new BenchResult("order-book", cfg.ops, elapsed, eng.checksum, samples)
                .stat("fills", eng.fillCount)
                .stat("fill_qty", eng.fillQty)
                .stat("cancel_hits", eng.cancelHits)
                .stat("cancel_misses", eng.cancelMisses)
                .stat("ioc_killed", eng.iocKilled)
                .stat("resting_adds", eng.restCount);
        printResult(r);
        return r;
    }

    private static BenchResult runFix(Config cfg) {
        log("generating FIX messages");
        FixBench bench = new FixBench(cfg.seed, cfg.ops, cfg.symbols);
        int warmup = Math.min(cfg.warmup, cfg.ops);
        log("warming fix-parse");
        for (int i = 0; i < warmup; i++) {
            bench.parse(i);
        }
        bench.checksum = 0;
        log("measuring fix-parse");
        long[] samples = new long[cfg.ops];
        long t0 = System.nanoTime();
        for (int i = 0; i < cfg.ops; i++) {
            long s = System.nanoTime();
            bench.parse(i);
            samples[i] = System.nanoTime() - s;
        }
        long elapsed = System.nanoTime() - t0;
        Percentiles.sortInPlace(samples);
        BenchResult r = new BenchResult("fix-parse", cfg.ops, elapsed, bench.checksum, samples)
                .stat("messages", cfg.ops)
                .stat("bytes", bench.arena.length);
        printResult(r);
        return r;
    }

    private static BenchResult runRisk(Config cfg) {
        log("generating risk workload");
        Workload w = Workload.generate(cfg.seed, cfg.ops, cfg.symbols, cfg.accounts);
        int warmup = Math.min(cfg.warmup, cfg.ops);
        log("warming risk");
        RiskEngine throwaway = new RiskEngine(cfg.accounts, cfg.symbols);
        for (int i = 0; i < warmup; i++) {
            throwaway.apply(w, i);
        }
        throwaway = null;
        System.gc();
        log("measuring risk");
        RiskEngine eng = new RiskEngine(cfg.accounts, cfg.symbols);
        long[] samples = new long[cfg.ops];
        long t0 = System.nanoTime();
        for (int i = 0; i < cfg.ops; i++) {
            long s = System.nanoTime();
            eng.apply(w, i);
            samples[i] = System.nanoTime() - s;
        }
        long elapsed = System.nanoTime() - t0;
        eng.finish();
        Percentiles.sortInPlace(samples);
        BenchResult r = new BenchResult("risk", cfg.ops, elapsed, eng.checksum, samples)
                .stat("accepts", eng.accepts)
                .stat("rejects", eng.rejects);
        printResult(r);
        return r;
    }

    private static BenchResult runMarketData(Config cfg) {
        log("generating market-data ticks");
        MarketDataBench bench = new MarketDataBench(cfg.seed, cfg.ops, cfg.symbols);
        int warmup = Math.min(cfg.warmup, cfg.ops);
        log("warming market-data");
        for (int i = 0; i < warmup; i++) {
            bench.apply(i);
        }
        for (TickAgg a : bench.aggs) {
            a.reset();
        }
        bench.checksum = 0;
        log("measuring market-data");
        long[] samples = new long[cfg.ops];
        long t0 = System.nanoTime();
        for (int i = 0; i < cfg.ops; i++) {
            long s = System.nanoTime();
            bench.apply(i);
            samples[i] = System.nanoTime() - s;
        }
        long elapsed = System.nanoTime() - t0;
        bench.finish();
        Percentiles.sortInPlace(samples);
        BenchResult r = new BenchResult("market-data", cfg.ops, elapsed, bench.checksum, samples)
                .stat("ticks", cfg.ops);
        printResult(r);
        return r;
    }

    private static BenchResult runBinaryMd(Config cfg) {
        log("generating binary ticks");
        BinaryMdBench bench = new BinaryMdBench(cfg.seed, cfg.ops, cfg.symbols);
        int warmup = Math.min(cfg.warmup, cfg.ops);
        log("warming binary-md");
        for (int i = 0; i < warmup; i++) {
            bench.apply(i);
        }
        for (TickAgg a : bench.aggs) {
            a.reset();
        }
        bench.checksum = 0;
        log("measuring binary-md");
        long[] samples = new long[cfg.ops];
        long t0 = System.nanoTime();
        for (int i = 0; i < cfg.ops; i++) {
            long s = System.nanoTime();
            bench.apply(i);
            samples[i] = System.nanoTime() - s;
        }
        long elapsed = System.nanoTime() - t0;
        bench.finish();
        Percentiles.sortInPlace(samples);
        BenchResult r = new BenchResult("binary-md", cfg.ops, elapsed, bench.checksum, samples)
                .stat("ticks", cfg.ops)
                .stat("bytes", (long) cfg.ops * BinaryMdBench.REC);
        printResult(r);
        return r;
    }

    private static void printResult(BenchResult r) {
        System.out.printf(
                Locale.US,
                "=== %s ===  %.0f ops/s  p50=%d ns  p99=%d ns  checksum=%s%n",
                r.name,
                r.throughputOpsS,
                r.p50Ns,
                r.p99Ns,
                r.checksum);
    }

    private static void log(String msg) {
        System.out.println(msg);
    }

    static final class Config {
        int ops = 1_000_000;
        int warmup = 50_000;
        long seed = C.DEFAULT_SEED;
        int symbols = 32;
        int accounts = 256;
        Path output = Path.of("results", "java.json");
        String runtimeName = "unknown";
        String benches = "all";
        boolean help;

        boolean want(String name) {
            if ("all".equals(benches)) {
                return true;
            }
            for (String p : benches.split(",")) {
                if (name.equals(p.trim())) {
                    return true;
                }
            }
            return false;
        }

        static Config parse(String[] args) {
            Config c = new Config();
            for (int i = 0; i < args.length; i++) {
                String a = args[i];
                switch (a) {
                    case "-h", "--help" -> c.help = true;
                    case "--ops" -> c.ops = Integer.parseInt(args[++i]);
                    case "--warmup" -> c.warmup = Integer.parseInt(args[++i]);
                    case "--seed" -> c.seed = parseSeed(args[++i]);
                    case "--symbols" -> c.symbols = Integer.parseInt(args[++i]);
                    case "--accounts" -> c.accounts = Integer.parseInt(args[++i]);
                    case "--output" -> c.output = Path.of(args[++i]);
                    case "--runtime-name" -> c.runtimeName = args[++i];
                    case "--bench" -> c.benches = args[++i];
                    default -> throw new IllegalArgumentException("unknown arg: " + a);
                }
            }
            if (c.ops <= 0 || c.symbols <= 0 || c.accounts <= 0) {
                throw new IllegalArgumentException("ops/symbols/accounts must be > 0");
            }
            return c;
        }

        static long parseSeed(String s) {
            if (s.startsWith("0x") || s.startsWith("0X")) {
                return Long.parseUnsignedLong(s.substring(2), 16);
            }
            return Long.parseUnsignedLong(s);
        }

        static void printHelp() {
            System.out.println(
                    """
                    java-bench — equities Java 21 harness
                      --ops N              operations per bench (default 1000000)
                      --warmup N           warmup ops (default 50000)
                      --seed HEX_OR_DEC    default 0x0C0FFEE123456789
                      --symbols N          default 32
                      --accounts N         default 256
                      --bench all|name,... order-book,fix-parse,risk,market-data,binary-md
                      --output PATH
                      --runtime-name NAME  docker|podman|host
                    """);
        }
    }

    @SuppressWarnings("unused")
    private static final Set<String> NAMES =
            Set.of("order-book", "fix-parse", "risk", "market-data", "binary-md");
}
