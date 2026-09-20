import math


def sort_in_place(samples: list[int]) -> None:
    samples.sort()


def pct(sorted_samples: list[int], p: float) -> int:
    if not sorted_samples:
        return 0
    i = int(math.floor(p * (len(sorted_samples) - 1)))
    if i < 0:
        i = 0
    if i >= len(sorted_samples):
        i = len(sorted_samples) - 1
    return sorted_samples[i]


def min_v(sorted_samples: list[int]) -> int:
    return sorted_samples[0] if sorted_samples else 0


def max_v(sorted_samples: list[int]) -> int:
    return sorted_samples[-1] if sorted_samples else 0
