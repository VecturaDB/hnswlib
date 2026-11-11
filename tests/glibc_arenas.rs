// The buffers of an index resized and dropped again and again leave no memory in the arenas of glibc.
#![cfg(all(target_os = "linux", target_env = "gnu"))]

use hnswlib::{HnswDistanceFunction, HnswIndex, HnswIndexInitConfig};

#[repr(C)]
struct Mallinfo2 {
    arena: usize,
    ordblks: usize,
    smblks: usize,
    hblks: usize,
    hblkhd: usize,
    usmblks: usize,
    fsmblks: usize,
    uordblks: usize,
    fordblks: usize,
    keepcost: usize,
}

extern "C" {
    fn mallinfo2() -> Mallinfo2;
}

fn arenas() -> usize {
    unsafe { mallinfo2() }.arena
}

// 64 dimensions and m 16: about 400 bytes of base layer for each element.
fn index_resized_to(elements: usize) {
    let mut index = HnswIndex::init(HnswIndexInitConfig {
        distance_function: HnswDistanceFunction::Euclidean,
        dimensionality: 64,
        persist_path: None,
        max_elements: 1000,
        m: 16,
        ef_construction: 100,
        ef_search: 10,
        random_seed: 0,
    })
    .unwrap();
    index.resize(elements).unwrap();
    index.add(0, &[0.5; 64]).unwrap();
}

// Once glibc has freed a block mapped on its own it raises its mmap threshold to that size and serves smaller blocks from
// its arenas, which keep them after the free: after an index of about 24 MB, the base layers of 16 MB of the next indexes
// would stay there.
#[test]
fn indexes_resized_and_dropped_leave_the_arenas_of_glibc_as_they_were() {
    let before = arenas();
    index_resized_to(60_000);
    for _ in 0..5 {
        index_resized_to(40_000);
    }
    let after = arenas();
    assert!(
        after < before + (8 << 20),
        "the arenas of glibc grew from {before} to {after} bytes"
    );
}
