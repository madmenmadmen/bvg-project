use std::collections::HashMap;
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
    pub chunks: HashMap<String, Vec<u8>>,
    pub order: Vec<String>,
    pub meta: HashMap<String, ChunkMeta>,
}

impl BvgFile {
    pub fn new(w: u32, h: u32) -> Self {
        Self { width: w, height: h,
               chunks: HashMap::new(), order: Vec::new(),
               meta: HashMap::new() }
    }

    pub fn set_chunk(&mut self, t: &str, data: Vec<u8>,
                     codec: u8, level: u8, use_pre: bool) {
        if !self.chunks.contains_key(t) {
            self.order.push(t.to_string());
        }
        self.chunks.insert(t.to_string(), data);
        self.meta.insert(t.to_string(), ChunkMeta { codec, level, use_pre });
    }

    pub fn get_chunk(&self, t: &str) -> Option<&Vec<u8>> {
        self.chunks.get(t)
    }

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
        let entries = unpack_chunk_table(data, table_off,
                                         h.chunk_count, per_chunk)?;
        let mut f = Self::new(h.width, h.height);
        for e in entries {
            let t = std::str::from_utf8(&e.type_)
                .map_err(|_| BvgError::chunk("bad chunk type"))?
                .to_string();
            let cid = if per_chunk { e.codec_id } else { h.codec_id };
            let lv = if per_chunk { e.level } else { h.level };
            let pre = if per_chunk { e.flags & CHUNK_PRE_RLE != 0 } else { false };
            let abs = data_start + e.offset as usize;
            if abs + e.size as usize > data.len() {
                return Err(BvgError::chunk("chunk out of bounds"));
            }
            let comp = &data[abs..abs + e.size as usize];
            let raw = codec::decompress(comp, cid, lv, pre,
                                        h.total_uncompressed as usize)?;
            f.set_chunk(&t, raw, cid, lv, pre);
        }
        Ok(f)
    }

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
            let raw = match self.chunks.get(t) {
                Some(r) => r,
                None => continue,
            };
            let mut cid = def_codec;
            let mut lv = def_level;
            let mut pre = rle_pre;
            if per_chunk {
                if let Some(m) = self.meta.get(t) {
                    cid = m.codec; lv = m.level; pre = m.use_pre;
                }
            } else {
                pre = false;
            }
            if first { global_codec = cid; global_level = lv; first = false; }
            let comp = codec::compress(raw, cid, lv, pre)?;
            let mut type_ = [0u8; 4];
            type_.copy_from_slice(&t.as_bytes()[..4]);
            entries.push(ChunkEntry {
                type_, offset: off, size: comp.len() as u32,
                codec_id: cid, level: lv,
                flags: if pre { CHUNK_PRE_RLE } else { 0 },
            });
            off += comp.len() as u32;
            total_raw += raw.len() as u64;
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