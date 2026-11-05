use std::collections::{HashMap, HashSet};
use std::path::Path;
use crate::generated::*;
use crate::codec;
use crate::BvgError;

#[derive(Debug, Clone)]
pub struct ChunkMeta {
    pub codec: u8,
    pub level: u8,
    pub use_pre: bool,
}

pub struct BvgFile {
    pub width: u32,
    pub height: u32,
    pub chunks: HashMap<String, Vec<u8>>,   // кэш decoded
    pub order: Vec<String>,
    pub meta: HashMap<String, ChunkMeta>,

    // --- lazy state ---
    raw: Option<Vec<u8>>,                   // исходные байты файла
    entries: Vec<ChunkEntry>,               // таблица чанков
    data_start: usize,                      // начало области данных
    per_chunk: bool,
    header_codec: u8,
    header_level: u8,
    dirty: HashSet<String>,                 // чанки, изменённые через set_chunk
}

impl BvgFile {
    pub fn new(w: u32, h: u32) -> Self {
        Self {
            width: w, height: h,
            chunks: HashMap::new(), order: Vec::new(),
            meta: HashMap::new(),
            raw: None, entries: Vec::new(), data_start: 0,
            per_chunk: false, header_codec: 0, header_level: 0,
            dirty: HashSet::new(),
        }
    }

    pub fn set_chunk(&mut self, t: &str, data: Vec<u8>,
                     codec: u8, level: u8, use_pre: bool) {
        if !self.chunks.contains_key(t) {
            self.order.push(t.to_string());
        }
        self.chunks.insert(t.to_string(), data);
        self.meta.insert(t.to_string(),
                         ChunkMeta { codec, level, use_pre });
        self.dirty.insert(t.to_string());
    }

    /// Lazy: декомпрессит только `t`, кэширует в `chunks`.
    /// Требует `&mut self` (мутирует кэш).
    pub fn get_chunk(&mut self, t: &str) -> Option<&Vec<u8>> {
        if self.chunks.contains_key(t) {
            return self.chunks.get(t);
        }
        let raw = self.raw.as_ref()?;
        for e in &self.entries {
            let name = std::str::from_utf8(&e.type_).ok()?;
            if name != t { continue; }
            let cid = if self.per_chunk { e.codec_id } else { self.header_codec };
            let lv  = if self.per_chunk { e.level }    else { self.header_level };
            let pre = self.per_chunk && (e.flags & CHUNK_PRE_RLE != 0);
            let abs = self.data_start + e.offset as usize;
            if abs + e.size as usize > raw.len() {
                // нельзя вернуть Err из Option — паникуем или логируем;
                // для корректного API можно вернуть Result, но
                // сохраняем совместимость с get_chunk -> Option
                return None;
            }
            let comp = &raw[abs..abs + e.size as usize];
            // raw_hint = 0: decompress сам определяет размер (zstd/lz4)
            let decoded = match codec::decompress(comp, cid, lv, pre, 0) {
                Ok(v) => v,
                Err(_) => return None,
            };
            self.chunks.insert(t.to_string(), decoded);
            return self.chunks.get(t);
        }
        None
    }

    /// Lazy: только header + chunk table. Payload'ы не декомпрессируются.
    pub fn read<P: AsRef<Path>>(p: P) -> Result<Self, BvgError> {
        let data = std::fs::read(p)
            .map_err(|e| BvgError::format(e.to_string()))?;
        Self::read_from_bytes(&data)
    }

    pub fn read_from_bytes(data: &[u8]) -> Result<Self, BvgError> {
        let h = Header::unpack(data)?;
        let per_chunk = h.flags & FLAG_PERCHUNK != 0;
        let rec = if per_chunk { CHUNK_SIZE_P } else { CHUNK_SIZE_S };
        let table_off = HEADER_SIZE;
        let data_start = table_off + rec * h.chunk_count as usize;
        if data_start > data.len() {
            return Err(BvgError::format("chunk table truncated"));
        }
        let entries = unpack_chunk_table(data, table_off,
                                         h.chunk_count, per_chunk)?;

        let mut f = Self::new(h.width, h.height);
        f.raw = Some(data.to_vec());
        f.entries = entries;
        f.data_start = data_start;
        f.per_chunk = per_chunk;
        f.header_codec = h.codec_id;
        f.header_level = h.level;

        for e in &f.entries {
            let t = std::str::from_utf8(&e.type_)
                .map_err(|_| BvgError::chunk("bad chunk type"))?
                .to_string();
            let cid = if per_chunk { e.codec_id } else { h.codec_id };
            let lv  = if per_chunk { e.level }    else { h.level };
            let pre = per_chunk && (e.flags & CHUNK_PRE_RLE != 0);
            f.order.push(t.clone());
            f.meta.insert(t, ChunkMeta { codec: cid, level: lv, use_pre: pre });
        }
        Ok(f)
    }

    /// dirty кодирует, остальные копирует байт-в-байт из raw.
    /// Если raw == None (файл создан с нуля) — кодирует всё.
    pub fn write<P: AsRef<Path>>(&self, p: P, per_chunk: bool,
                                 rle_pre: bool, def_codec: u8,
                                 def_level: u8)
        -> Result<(), BvgError>
    {
        let mut entries = Vec::new();
        let mut comps = Vec::new();
        let mut total_raw = 0u64;
        let mut off = 0u32;
        let mut first = true;
        let mut global_codec = 0u8;
        let mut global_level = 0u8;

        for t in &self.order {
            let mut comp: Vec<u8>;
            let cid: u8;
            let lv: u8;
            let pre: bool;

            if self.raw.is_some() && !self.dirty.contains(t) {
                // Копируем байт-в-байт из исходного файла.
                let ent = match self.entries.iter()
                    .find(|e| std::str::from_utf8(&e.type_).ok() == Some(t.as_str()))
                {
                    Some(e) => e,
                    None => continue,
                };
                let raw_buf = self.raw.as_ref().unwrap();
                let abs = self.data_start + ent.offset as usize;
                if abs + ent.size as usize > raw_buf.len() {
                    return Err(BvgError::chunk("chunk out of bounds"));
                }
                comp = raw_buf[abs..abs + ent.size as usize].to_vec();
                cid = if per_chunk { ent.codec_id } else { self.header_codec };
                lv  = if per_chunk { ent.level }    else { self.header_level };
                pre = per_chunk && (ent.flags & CHUNK_PRE_RLE != 0);

                // raw_len для total_uncompressed — декомпрессим только для подсчёта.
                let decoded = codec::decompress(&comp, cid, lv, pre, 0)?;
                total_raw += decoded.len() as u64;
            } else {
                // Кодируем из chunks[t].
                let raw_payload = match self.chunks.get(t) {
                    Some(r) => r,
                    None => continue,
                };
                let mut c = def_codec;
                let mut l = def_level;
                let mut pr = rle_pre;
                if per_chunk {
                    if let Some(m) = self.meta.get(t) {
                        c = m.codec; l = m.level; pr = m.use_pre;
                    }
                } else {
                    pr = false;
                }
                cid = c; lv = l; pre = pr;
                comp = codec::compress(raw_payload, cid, lv, pre)?;
                total_raw += raw_payload.len() as u64;
            }

            if first { global_codec = cid; global_level = lv; first = false; }
            let mut type_ = [0u8; 4];
            type_.copy_from_slice(&t.as_bytes()[..4]);
            entries.push(ChunkEntry {
                type_, offset: off, size: comp.len() as u32,
                codec_id: cid, level: lv,
                flags: if pre { CHUNK_PRE_RLE } else { 0 },
            });
            off += comp.len() as u32;
            comps.push(comp);
        }

        // SPEC 7: FLAG_U32XY is always set by writers in v1
        let mut flags = FLAG_U32XY;
        if per_chunk { flags |= FLAG_PERCHUNK; }
        if self.chunks.contains_key("THMB") { flags |= FLAG_HAS_THUMB; }
        if self.chunks.contains_key("META") { flags |= FLAG_HAS_META; }

        let h = Header {
            version: VERSION, flags,
            width: self.width, height: self.height,
            chunk_count: entries.len() as u32,
            codec_id: if per_chunk { 0 } else { global_codec },
            level: if per_chunk { 0 } else { global_level },
            total_uncompressed: total_raw,
        };

        let mut out = h.pack();
        out.extend(pack_chunk_table(&entries, per_chunk));
        for c in comps { out.extend_from_slice(&c); }
        std::fs::write(p, out)
            .map_err(|e| BvgError::format(e.to_string()))
    }
}