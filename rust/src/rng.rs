use crate::constants as c;

pub struct XorShift64 {
    state: u64,
}

impl XorShift64 {
    pub fn new(seed: u64) -> Self {
        Self {
            state: if seed == 0 { c::DEFAULT_SEED } else { seed },
        }
    }

    pub fn next_u64(&mut self) -> u64 {
        let mut x = self.state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.state = x;
        x
    }

    pub fn next_bounded(&mut self, n: u32) -> u32 {
        debug_assert!(n > 0);
        (self.next_u64() % u64::from(n)) as u32
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn same_seed_same_sequence() {
        let mut a = XorShift64::new(c::DEFAULT_SEED);
        let mut b = XorShift64::new(c::DEFAULT_SEED);
        for _ in 0..100 {
            assert_eq!(a.next_u64(), b.next_u64());
        }
    }

    #[test]
    fn zero_seed_uses_default() {
        let mut a = XorShift64::new(0);
        let mut b = XorShift64::new(c::DEFAULT_SEED);
        assert_eq!(a.next_u64(), b.next_u64());
    }
}
