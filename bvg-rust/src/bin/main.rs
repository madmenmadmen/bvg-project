use std::env;
use std::process::ExitCode;

// Добавлен recompress
use bvg::cli::{dump, info, list, codecs, recompress};

fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        usage();
        return ExitCode::from(2);
    }
    let cmd = &args[1];
    let rest = &args[2..];
    match cmd.as_str() {
        "info"       => info::run(rest),
        "list"       => list::run(rest),
        "dump"       => dump::run(rest),
        "codecs"     => codecs::run(rest),
        "recompress" => recompress::run(rest),
        "--version" | "-V" => {
            println!("bvg {}", env!("CARGO_PKG_VERSION"));
            ExitCode::SUCCESS
        }
        "--help" | "-h" => {
            usage();
            ExitCode::SUCCESS
        }
        _ => {
            eprintln!("unknown command: {}", cmd);
            usage();
            ExitCode::from(2)
        }
    }
}

fn usage() {
    eprintln!("usage: bvg-rust <command> [args]");
    eprintln!("commands:");
    eprintln!("  info <file>              print header info");
    eprintln!("  list <file>              list chunks");
    eprintln!("  dump <file> [-o out]     dump contents as JSON");
    eprintln!("  recompress <in> <out>    re-encode BVG file");
    eprintln!("  codecs                   list codecs");
    eprintln!("  --version                print version");
}