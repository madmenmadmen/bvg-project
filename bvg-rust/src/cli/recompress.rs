use std::fs;
use std::process::ExitCode;

use crate::file::BvgFile;
use crate::codec;

pub fn run(args: &[String]) -> ExitCode {
    let mut input = None;
    let mut output = None;
    let mut codec_name = "auto".to_string();
    let mut level: u8 = 0;
    let mut per_chunk = true;
    let mut rle_pre = true;

    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "--codec" if i + 1 < args.len() => { codec_name = args[i+1].clone(); i += 2; }
            "--level" if i + 1 < args.len() => { level = args[i+1].parse().unwrap_or(0); i += 2; }
            "--global-codec" => { per_chunk = false; i += 1; }
            "--per-chunk"    => { per_chunk = true; i += 1; }
            "--no-rle-pre"   => { rle_pre = false; i += 1; }
            _ => {
                if input.is_none() { input = Some(args[i].clone()); }
                else { output = Some(args[i].clone()); }
                i += 1;
            }
        }
    }
    let (input, output) = match (input, output) {
        (Some(i), Some(o)) => (i, o),
        _ => {
            eprintln!("usage: bvg recompress <in> <out> [--codec X] [--level N]");
            return ExitCode::from(2);
        }
    };

    let cid: Option<u8> = if codec_name == "auto" {
        None
    } else {
        match codec::id_from_name(&codec_name) {
            Some(c) => Some(c),
            None => {
                eprintln!("unknown codec: {}", codec_name);
                return ExitCode::from(2);
            }
        }
    };

    let mut f = match BvgFile::read(&input) {
        Ok(f) => f,
        Err(e) => { eprintln!("error: {:?}", e); return ExitCode::from(1); }
    };
    for t in &f.order {
        f.meta.insert(t.clone(), crate::file::ChunkMeta {
            codec: cid.unwrap_or(0),
            level,
            use_pre: rle_pre,
        });
    }
    if let Err(e) = f.write(&output, per_chunk, rle_pre,
                            cid.unwrap_or(0), level) {
        eprintln!("error: {:?}", e);
        return ExitCode::from(1);
    }
    let in_size = fs::metadata(&input).map(|m| m.len()).unwrap_or(0);
    let out_size = fs::metadata(&output).map(|m| m.len()).unwrap_or(0);
    let ratio = if out_size > 0 { in_size as f64 / out_size as f64 } else { 0.0 };
    println!("{}  {} B", input, in_size);
    println!("{}  {} B  ({:.2}x smaller)", output, out_size, ratio);
    ExitCode::SUCCESS
}