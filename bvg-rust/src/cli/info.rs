use std::fs;
use std::process::ExitCode;

use crate::generated::{self, Header};
use crate::codec;

pub fn run(args: &[String]) -> ExitCode {
    if args.is_empty() {
        eprintln!("usage: bvg-rust info <file>");
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
    let has_thumb = h.flags & generated::FLAG_HAS_THUMB != 0;
    let has_meta = h.flags & generated::FLAG_HAS_META != 0;

    println!("BVG v{}  {}", h.version, path);
    println!("  format kind          vector");
    println!("  magic                b'BVG\\x00'");
    println!("  canvas               {} x {}", h.width, h.height);
    println!("  chunks               {}", h.chunk_count);
    println!("  global codec         {} (id={}, level={})",
             codec::name(h.codec_id), h.codec_id, h.level);
    println!("  per-chunk            {}", per_chunk);
    println!("  has thumb            {}", has_thumb);
    println!("  has meta             {}", has_meta);
    println!("  total uncompressed   {} B", h.total_uncompressed);
    if let Ok(md) = fs::metadata(path) {
        println!("  file size            {} B", md.len());
    }
    ExitCode::SUCCESS
}