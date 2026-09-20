import unittest

from equities_bench import constants as c
from equities_bench.book import OrderBookEngine
from equities_bench.md import BinaryMdBench, MarketDataBench
from equities_bench.rng import XorShift64
from equities_bench.workload import Workload


class RngTest(unittest.TestCase):
    def test_same_seed(self) -> None:
        a = XorShift64(c.DEFAULT_SEED)
        b = XorShift64(c.DEFAULT_SEED)
        for _ in range(100):
            self.assertEqual(a.next_u64(), b.next_u64())

    def test_zero_seed(self) -> None:
        a = XorShift64(0)
        b = XorShift64(c.DEFAULT_SEED)
        self.assertEqual(a.next_u64(), b.next_u64())


class OrderBookTest(unittest.TestCase):
    def test_match(self) -> None:
        eng = OrderBookEngine(1, 8)
        eng.add(1, 0, c.SELL, 10_000, 10, c.TIF_GTC)
        eng.add(2, 0, c.BUY, 10_000, 10, c.TIF_GTC)
        self.assertEqual(eng.fill_count, 1)
        self.assertEqual(eng.fill_qty, 10)

    def test_deterministic(self) -> None:
        w = Workload.generate(c.DEFAULT_SEED, 5_000, 8, 16)
        a = OrderBookEngine(8, 5_000)
        b = OrderBookEngine(8, 5_000)
        for i in range(w.n_ops):
            a.apply(w, i)
            b.apply(w, i)
        a.finish()
        b.finish()
        self.assertEqual(a.checksum, b.checksum)
        self.assertGreater(a.fill_count, 0)


class MdTest(unittest.TestCase):
    def test_binary_matches_ticks(self) -> None:
        md = MarketDataBench(c.DEFAULT_SEED, 2_000, 8)
        bin_ = BinaryMdBench(c.DEFAULT_SEED, 2_000, 8)
        for i in range(2_000):
            md.apply(i)
            bin_.apply(i)
        md.finish()
        bin_.finish()
        self.assertEqual(md.checksum, bin_.checksum)


if __name__ == "__main__":
    unittest.main()
