use std::fs;
use std::process::ExitCode;

use crate::generated::{self, Header};
use crate::codec;

pub fn run(args: &[String]) -> ExitCode {
    if args.is_empty() {
        eprintln!("usage: bvg-rust list <file>");
        return ExitCode::from(2);
    }
    let path = &args[0];
    let data = match fs::read(path) {
        Ok(d) => d,
        Err(e) => {
            eprintln!("error: {}", e);
            return ExitCode::from(1);
        }
    };
    let h = match Header::unpack(&data) {
        Ok(h) => h,
        Err(e) => {
            eprintln!("error: {:?}", e);
            return ExitCode::from(1);
        }
    };
    let per_chunk = h.flags & generated::FLAG_PERCHUNK != 0;
    let entries = match generated::unpack_chunk_table(
        &data, generated::HEADER_SIZE, h.chunk_count, per_chunk,
    ) {
        Ok(e) => e,
        Err(e) => {
            eprintln!("error: {:?}", e);
            return ExitCode::from(1);
        }
    };
    let rec = if per_chunk { generated::CHUNK_SIZE_P }
              else { generated::CHUNK_SIZE_S };
    let data_start = generated::HEADER_SIZE + rec * h.chunk_count as usize;

    println!("BVG v{}  {}  {}x{}  chunks={}  per_chunk={}",
             h.version, path, h.width, h.height, h.chunk_count, per_chunk);
    println!();
    println!("{:<6} {:<6} {:>4} {:>4} {:>10} {:>10} {:>9}",
             "type", "codec", "lvl", "pre", "size", "raw", "comp/raw");
    println!("{}", "-".repeat(60));

    let mut total_c: u64 = 0;
    let mut total_r: u64 = 0;
    for e in &entries {
        let t = String::from_utf8_lossy(&e.type_);
        let cid = if per_chunk { e.codec_id } else { h.codec_id };
        let lv = if per_chunk { e.level } else { h.level };
        let pre = per_chunk && (e.flags & generated::CHUNK_PRE_RLE != 0);
        let comp = &data[data_start + e.offset as usize
                         ..data_start + e.offset as usize + e.size as usize];
        let raw_size: i64 = match codec::decompress(comp, cid, lv, pre, 0) {
            Ok(raw) => raw.len() as i64,
            Err(_) => -1,
        };
        let ratio = if raw_size > 0 {
            e.size as f64 / raw_size as f64
        } else {
            0.0
        };
        println!("{:<6} {:<6} {:>4} {:>4} {:>10} {:>10} {:>8.2}x",
                 t, codec::name(cid), lv, if pre { "yes" } else { "no" },
                 e.size, raw_size.max(0), ratio);
        total_c += e.size as u64;
        if raw_size > 0 {
            total_r += raw_size as u64;
        }
    }
    println!("{}", "-".repeat(60));
    let ratio = if total_r > 0 {
        total_c as f64 / total_r as f64
    } else {
        0.0
    };
    println!("{:<6} {:<6} {:>4} {:>4} {:>10} {:>10} {:>8.2}x",
             "TOTAL", "", "", "", total_c, total_r, ratio);
    ExitCode::SUCCESS
}