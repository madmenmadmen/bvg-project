use std::fs;
use std::process::ExitCode;

use crate::file::BvgFile;

fn hex(data: &[u8]) -> String {
    let mut s = String::with_capacity(data.len() * 2);
    for b in data {
        s.push_str(&format!("{:02x}", b));
    }
    s
}

pub fn run(args: &[String]) -> ExitCode {
    if args.is_empty() {
        eprintln!("usage: bvg-rust dump <file> [-o out.json]");
        return ExitCode::from(2);
    }
    let path = &args[0];
    let mut out: Option<&str> = None;
    let mut i = 1;
    while i < args.len() {
        if args[i] == "-o" && i + 1 < args.len() {
            out = Some(&args[i + 1]);
            i += 2;
        } else {
            i += 1;
        }
    }

    let data = match fs::read(path) {
        Ok(d) => d,
        Err(e) => {
            eprintln!("error: {}", e);
            return ExitCode::from(1);
        }
    };
    let mut f = match BvgFile::read_from_bytes(&data) {
        Ok(f) => f,
        Err(e) => {
            eprintln!("error: {:?}", e);
            return ExitCode::from(1);
        }
    };

    // snapshot meta до get_chunk (borrow checker)
    let meta_snapshot: Vec<(String, u8, u8, bool)> = f.order.iter()
        .map(|t| {
            let m = f.meta.get(t);
            (t.clone(),
             m.map(|m| m.codec).unwrap_or(0),
             m.map(|m| m.level).unwrap_or(0),
             m.map(|m| m.use_pre).unwrap_or(false))
        })
        .collect();

    let mut js = String::new();
    js.push_str("{\n  \"chunks\": [\n");
    let mut first = true;
    for (t, codec, level, use_pre) in &meta_snapshot {
        let raw = match f.get_chunk(t) {
            Some(r) => r,
            None => continue,
        };
        if !first {
            js.push_str(",\n");
        }
        first = false;
        js.push_str("    {\n");
        js.push_str(&format!("      \"hex\": \"{}\",\n", hex(raw)));
        js.push_str("      \"meta\": {\n");
        js.push_str(&format!("        \"codec\": {},\n", codec));
        js.push_str(&format!("        \"level\": {},\n", level));
        js.push_str(&format!("        \"use_pre\": {}\n", use_pre));
        js.push_str("      },\n");
        js.push_str(&format!("      \"type\": \"{}\"\n", t));
        js.push_str("    }");
    }
    js.push_str("\n  ],\n");
    let base = std::path::Path::new(path)
        .file_name()
        .unwrap()
        .to_string_lossy();
    js.push_str(&format!("  \"file\": \"{}\",\n", base));
    js.push_str(&format!("  \"height\": {},\n", f.height));
    js.push_str(&format!("  \"width\": {}\n", f.width));
    js.push_str("}\n");

    if let Some(o) = out {
        if let Err(e) = fs::write(o, &js) {
            eprintln!("error: {}", e);
            return ExitCode::from(1);
        }
        eprintln!("wrote {}", o);
    } else {
        print!("{}", js);
    }
    ExitCode::SUCCESS
}