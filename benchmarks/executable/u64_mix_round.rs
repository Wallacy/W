// Expected cases (user arguments => exit; stdout; stderr):
// [] => 0; "Mix 16743387770561346575\n"; ""
// ["x"] => 0; "Mix 553619412775969103\n"; ""
// ["alpha", "beta", "gamma"] => 0; "Mix 5608831001354178255\n"; ""

fn rotate_and_fold(value: u64) -> u64 {
    let rotated = value.rotate_left(23);
    let shifted = rotated >> 17;
    rotated ^ shifted
}

fn mix_round(state: u64, lane: u64) -> u64 {
    let added = state.wrapping_add(lane);
    let folded = rotate_and_fold(added);
    folded.wrapping_mul(0x9e3779b185ebca87)
}

fn main() {
    let lane = u64::try_from(std::env::args_os().skip(1).count())
        .expect("argument count does not fit u64");
    let mixed = mix_round(0x0206f2a26db56cfd, lane);
    println!("Mix {mixed}");
}
