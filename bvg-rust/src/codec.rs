use std::io::{Read, Write};
use crate::BvgError;

pub const NONE: u8 = 0;
pub const ZLIB: u8 = 1;
pub const ZSTD: u8 = 2;
pub const LZ4: u8 = 3;
pub const RLE: u8 = 4;

pub fn id_from_name(name: &str) -> Option<u8> {
    match name {
        "none" => Some(0),
        "zlib" => Some(1),
        "zstd" => Some(2),
        "lz4"  => Some(3),
        "rle"  => Some(4),
        _ => None,
    }
}

pub fn resolve_level(cid: u8, level: u8) -> i32 {
    let (lo, hi, def) = match cid {
        0 => (0, 0, 0),
        1 => (1, 9, 6),
        2 => (1, 22, 3),
        3 => (0, 16, 0),
        4 => (0, 0, 0),
        _ => (0, 0, 0),
    };
    if level == 0 { def } else { (level as i32).max(lo).min(hi) }
}

pub fn compress(data: &[u8], cid: u8, level: u8, rle_pre: bool)
    -> Result<Vec<u8>, BvgError>
{
    let work;
    let src = if rle_pre && cid != RLE {
        work = crate::rle::encode(data);
        work.as_slice()
    } else {
        data
    };
    let lv = resolve_level(cid, level);
    match cid {
        NONE => Ok(src.to_vec()),
        ZLIB => {
            let mut enc = flate2::write::ZlibEncoder::new(
                Vec::new(), flate2::Compression::new(lv as u32));
            enc.write_all(src).map_err(|e| BvgError::codec(e.to_string()))?;
            enc.finish().map_err(|e| BvgError::codec(e.to_string()))
        }
        ZSTD => zstd::encode_all(src, lv)
            .map_err(|e| BvgError::codec(e.to_string())),
        LZ4 => {
            let mut enc = lz4_flex::frame::FrameEncoder::new(Vec::new());
            enc.write_all(src).map_err(|e| BvgError::codec(e.to_string()))?;
            enc.finish().map_err(|e| BvgError::codec(e.to_string()))
        }
        RLE => Ok(crate::rle::encode(src)),
        _ => Err(BvgError::codec(format!("unknown codec: {}", cid))),
    }
}

/// Декомпрессия одного чанка.
///
/// `raw_hint` — опциональная подсказка (начальная ёмкость буфера).
/// Не обязательна: zstd/lz4 сами определяют размер из фрейма.
/// `total_uncompressed` из заголовка НЕ используется для декомпрессии
/// (по спеке это поле необязательное).
pub fn decompress(data: &[u8], cid: u8, level: u8, rle_pre: bool,
                  raw_hint: usize)
    -> Result<Vec<u8>, BvgError>
{
    let _ = level;  // уровень не нужен для декомпрессии
    let out = match cid {
        NONE => data.to_vec(),

        ZLIB => {
            let mut dec = flate2::read::ZlibDecoder::new(data);
            let mut out = Vec::with_capacity(if raw_hint > 0 { raw_hint } else { 0 });
            dec.read_to_end(&mut out)
                .map_err(|e| BvgError::codec(e.to_string()))?;
            out
        }

        ZSTD => {
            match zstd::zstd_safe::get_frame_content_size(data) {
                Ok(Some(n)) if n <= usize::MAX as u64 => {
                    let mut out = vec![0u8; n as usize];
                    let written = zstd::zstd_safe::decompress(&mut out, data)
                        .map_err(|e| BvgError::codec(e.to_string()))?;
                    out.truncate(written);
                    out
                }
                _ => {
                    let mut dec = zstd::stream::Decoder::new(data)
                        .map_err(|e| BvgError::codec(e.to_string()))?;
                    let mut out = Vec::with_capacity(
                        if raw_hint > 0 { raw_hint } else { 64 * 1024 });
                    dec.read_to_end(&mut out)
                        .map_err(|e| BvgError::codec(e.to_string()))?;
                    out
                }
            }
        }

        LZ4 => {
            let mut dec = lz4_flex::frame::FrameDecoder::new(data);
            let mut out = Vec::with_capacity(
                if raw_hint > 0 { raw_hint } else { 64 * 1024 });
            dec.read_to_end(&mut out)
                .map_err(|e| BvgError::codec(e.to_string()))?;
            out
        }

        RLE => return crate::rle::decode(data),

        _ => return Err(BvgError::codec(format!("unknown codec: {}", cid))),
    };

    if rle_pre && cid != RLE {
        return crate::rle::decode(&out);
    }
    Ok(out)
}

pub fn name(cid: u8) -> &'static str {
    match cid {
        0 => "none",
        1 => "zlib",
        2 => "zstd",
        3 => "lz4",
        4 => "rle",
        _ => "unknown",
    }
}