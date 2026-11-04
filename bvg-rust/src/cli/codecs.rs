use std::process::ExitCode;

use crate::codec;

pub fn run(_args: &[String]) -> ExitCode {
    println!("{:>3} {:<6} {:<10} {:>8} {:>10}",
             "id", "name", "available", "default", "range");
    println!("{}", "-".repeat(42));
    for cid in 0u8..=4 {
        let name = codec::name(cid);
        let lv = match cid {
            0 => 0,
            1 => 6,
            2 => 3,
            3 => 0,
            4 => 0,
            _ => 0,
        };
        let (lo, hi) = match cid {
            0 => (0, 0),
            1 => (1, 9),
            2 => (1, 22),
            3 => (0, 16),
            4 => (0, 0),
            _ => (0, 0),
        };
        let rng = if hi > 0 {
            format!("{}..{}", lo, hi)
        } else {
            "-".to_string()
        };
        println!("{:>3} {:<6} {:<10} {:>8} {:>10}",
                 cid, name, "yes", lv, rng);
    }
    ExitCode::SUCCESS
}