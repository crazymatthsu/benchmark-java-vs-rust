pub fn sort_in_place(samples: &mut [u64]) {
    samples.sort_unstable();
}

pub fn pct(sorted: &[u64], p: f64) -> u64 {
    if sorted.is_empty() {
        return 0;
    }
    let i = (p * (sorted.len() - 1) as f64).floor() as usize;
    sorted[i.min(sorted.len() - 1)]
}

pub fn min(sorted: &[u64]) -> u64 {
    sorted.first().copied().unwrap_or(0)
}

pub fn max(sorted: &[u64]) -> u64 {
    sorted.last().copied().unwrap_or(0)
}
