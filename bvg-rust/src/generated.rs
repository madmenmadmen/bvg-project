// GENERATED FILE - DO NOT EDIT
// Source: bvg-spec/schema.yaml
#![allow(dead_code)]

pub const MAGIC: [u8; 4] = [b'B', b'V', b'G', 0];
pub const VERSION: u16 = 1;
pub const HEADER_SIZE: usize = 32;
pub const CHUNK_SIZE_S: usize = 12;
pub const CHUNK_SIZE_P: usize = 15;

pub const FLAG_PERCHUNK: u16 = 1 << 0;
pub const FLAG_U32XY: u16 = 1 << 1;
pub const FLAG_HAS_THUMB: u16 = 1 << 2;
pub const FLAG_HAS_META: u16 = 1 << 3;

pub const CHUNK_PRE_RLE: u8 = 1 << 0;

// ---------- helpers ----------

fn need(b: &[u8], p: usize, n: usize, ctx: &str) -> Result<(), crate::BvgError> {
    if p + n > b.len() {
        return Err(crate::BvgError::chunk(format!("{}: truncated", ctx)));
    }
    Ok(())
}

fn put_u16(v: &mut Vec<u8>, x: u16) { v.extend_from_slice(&x.to_le_bytes()); }
fn put_u32(v: &mut Vec<u8>, x: u32) { v.extend_from_slice(&x.to_le_bytes()); }
fn put_u64(v: &mut Vec<u8>, x: u64) { v.extend_from_slice(&x.to_le_bytes()); }
fn put_f32(v: &mut Vec<u8>, x: f32) { v.extend_from_slice(&x.to_le_bytes()); }

fn get_u16(b: &[u8], p: &mut usize) -> u16 {
    let x = u16::from_le_bytes([b[*p], b[*p + 1]]);
    *p += 2; x
}
fn get_u32(b: &[u8], p: &mut usize) -> u32 {
    let x = u32::from_le_bytes([b[*p], b[*p+1], b[*p+2], b[*p+3]]);
    *p += 4; x
}
fn get_f32(b: &[u8], p: &mut usize) -> f32 {
    let x = f32::from_le_bytes([b[*p], b[*p+1], b[*p+2], b[*p+3]]);
    *p += 4; x
}

// ---------- header ----------

#[derive(Debug, Default, Clone)]
pub struct Header {
    pub version: u16,
    pub flags: u16,
    pub width: u32,
    pub height: u32,
    pub chunk_count: u32,
    pub codec_id: u8,
    pub level: u8,
    pub total_uncompressed: u64,
}

impl Header {
    pub fn pack(&self) -> Vec<u8> {
        let mut v = Vec::with_capacity(HEADER_SIZE);
        v.extend_from_slice(&MAGIC);
        put_u16(&mut v, self.version);
        put_u16(&mut v, self.flags);
        put_u32(&mut v, self.width);
        put_u32(&mut v, self.height);
        put_u32(&mut v, self.chunk_count);
        v.push(self.codec_id);
        v.push(self.level);
        put_u16(&mut v, 0);
        put_u64(&mut v, self.total_uncompressed);
        v
    }

    pub fn unpack(data: &[u8]) -> Result<Self, crate::BvgError> {
        if data.len() < HEADER_SIZE {
            return Err(crate::BvgError::format("file too small"));
        }
        if &data[0..4] != &MAGIC {
            return Err(crate::BvgError::format("bad magic"));
        }
        let version = u16::from_le_bytes([data[4], data[5]]);
        if version != VERSION {
            return Err(crate::BvgError::version("unsupported version"));
        }
        let reserved = u16::from_le_bytes([data[22], data[23]]);
        if reserved != 0 {
            return Err(crate::BvgError::format("reserved must be zero"));
        }
        Ok(Self {
            version,
            flags: u16::from_le_bytes([data[6], data[7]]),
            width: u32::from_le_bytes([data[8], data[9], data[10], data[11]]),
            height: u32::from_le_bytes([data[12], data[13], data[14], data[15]]),
            chunk_count: u32::from_le_bytes([data[16], data[17], data[18], data[19]]),
            codec_id: data[20],
            level: data[21],
            total_uncompressed: u64::from_le_bytes([
                data[24], data[25], data[26], data[27],
                data[28], data[29], data[30], data[31],
            ]),
        })
    }
}

// ---------- chunk table ----------

#[derive(Debug, Default, Clone)]
pub struct ChunkEntry {
    pub type_: [u8; 4],
    pub offset: u32,
    pub size: u32,
    pub codec_id: u8,
    pub level: u8,
    pub flags: u8,
}

pub fn pack_chunk_table(entries: &[ChunkEntry], per_chunk: bool) -> Vec<u8> {
    let rec = if per_chunk { CHUNK_SIZE_P } else { CHUNK_SIZE_S };
    let mut v = Vec::with_capacity(entries.len() * rec);
    for e in entries {
        v.extend_from_slice(&e.type_);
        put_u32(&mut v, e.offset);
        put_u32(&mut v, e.size);
        if per_chunk {
            v.push(e.codec_id);
            v.push(e.level);
            v.push(e.flags);
        }
    }
    v
}

pub fn unpack_chunk_table(
    data: &[u8], off: usize, count: u32, per_chunk: bool,
) -> Result<Vec<ChunkEntry>, crate::BvgError> {
    let rec = if per_chunk { CHUNK_SIZE_P } else { CHUNK_SIZE_S };
    let need_bytes = off + (count as usize) * rec;
    if need_bytes > data.len() {
        return Err(crate::BvgError::format("chunk table truncated"));
    }
    let mut out = Vec::with_capacity(count as usize);
    let mut p = off;
    for _ in 0..count {
        let mut type_ = [0u8; 4];
        type_.copy_from_slice(&data[p..p + 4]);
        p += 4;
        let offset = get_u32(data, &mut p);
        let size = get_u32(data, &mut p);
        let (codec_id, level, flags) = if per_chunk {
            let c = data[p]; let l = data[p+1]; let f = data[p+2];
            p += 3;
            (c, l, f)
        } else { (0, 0, 0) };
        out.push(ChunkEntry { type_, offset, size, codec_id, level, flags });
    }
    Ok(out)
}

// ---------- COLR ----------

pub fn pack_colr(colors: &[[u8; 4]]) -> Vec<u8> {
    let mut v = Vec::with_capacity(2 + colors.len() * 4);
    put_u16(&mut v, colors.len() as u16);
    for c in colors {
        v.extend_from_slice(c);
    }
    v
}

pub fn unpack_colr(data: &[u8]) -> Result<Vec<[u8; 4]>, crate::BvgError> {
    need(data, 0, 2, "COLR")?;
    let mut p = 0usize;
    let n = get_u16(data, &mut p) as usize;
    need(data, p, n * 4, "COLR")?;
    let mut out = Vec::with_capacity(n);
    for _ in 0..n {
        out.push([data[p], data[p+1], data[p+2], data[p+3]]);
        p += 4;
    }
    Ok(out)
}

// ---------- GRAD ----------

#[derive(Debug, Default, Clone)]
pub struct GradientStop {
    pub offset: f32,
    pub color_idx: u16,
    pub rgba: [u8; 4],
}

#[derive(Debug, Default, Clone)]
pub struct Gradient {
    pub kind: u8,
    pub flags: u8,
    pub stops: Vec<GradientStop>,
    pub geometry: Vec<f64>,
    pub transform: Option<[f64; 6]>,
}

pub const GRAD_LINEAR: u8 = 0;
pub const GRAD_RADIAL: u8 = 1;
pub const GRAD_CONIC: u8 = 2;
pub const GRAD_PATTERN: u8 = 3;

pub fn pack_grad(grads: &[Gradient]) -> Vec<u8> {
    let mut v = Vec::new();
    put_u16(&mut v, grads.len() as u16);
    for g in grads {
        let flags = if g.transform.is_some() { 1u8 } else { 0u8 };
        v.push(g.kind);
        v.push(flags);
        put_u16(&mut v, g.stops.len() as u16);
        for s in &g.stops {
            put_f32(&mut v, s.offset);
            put_u16(&mut v, s.color_idx);
            if s.color_idx == 0xFFFF {
                v.extend_from_slice(&s.rgba);
            }
        }
        match g.kind {
            GRAD_LINEAR | GRAD_CONIC => {
                for i in 0..4 { put_f32(&mut v, g.geometry[i] as f32); }
            }
            GRAD_RADIAL => {
                for i in 0..5 { put_f32(&mut v, g.geometry[i] as f32); }
            }
            GRAD_PATTERN => {
                put_u16(&mut v, g.geometry[0] as u16);
                for i in 1..5 { put_f32(&mut v, g.geometry[i] as f32); }
            }
            _ => {}
        }
        if flags == 1 {
            for i in 0..6 { put_f32(&mut v, g.transform.unwrap()[i] as f32); }
        }
    }
    v
}

pub fn unpack_grad(data: &[u8]) -> Result<Vec<Gradient>, crate::BvgError> {
    need(data, 0, 2, "GRAD")?;
    let mut p = 0usize;
    let n = get_u16(data, &mut p) as usize;
    let mut out = Vec::with_capacity(n);
    for _ in 0..n {
        need(data, p, 4, "GRAD")?;
        let kind = data[p]; p += 1;
        let flags = data[p]; p += 1;
        let sc = get_u16(data, &mut p) as usize;
        let mut stops = Vec::with_capacity(sc);
        for _ in 0..sc {
            need(data, p, 6, "GRAD")?;
            let offset = get_f32(data, &mut p);
            let color_idx = get_u16(data, &mut p);
            let mut rgba = [0u8; 4];
            if color_idx == 0xFFFF {
                need(data, p, 4, "GRAD")?;
                rgba.copy_from_slice(&data[p..p+4]);
                p += 4;
            }
            stops.push(GradientStop { offset, color_idx, rgba });
        }
        let geometry: Vec<f64> = match kind {
            GRAD_LINEAR | GRAD_CONIC => {
                need(data, p, 16, "GRAD")?;
                let mut g = Vec::with_capacity(4);
                for _ in 0..4 { g.push(get_f32(data, &mut p) as f64); }
                g
            }
            GRAD_RADIAL => {
                need(data, p, 20, "GRAD")?;
                let mut g = Vec::with_capacity(5);
                for _ in 0..5 { g.push(get_f32(data, &mut p) as f64); }
                g
            }
            GRAD_PATTERN => {
                need(data, p, 18, "GRAD")?;
                let mut g = Vec::with_capacity(5);
                g.push(get_u16(data, &mut p) as f64);
                for _ in 0..4 { g.push(get_f32(data, &mut p) as f64); }
                g
            }
            _ => return Err(crate::BvgError::chunk("GRAD: bad kind")),
        };
        let transform = if flags & 1 != 0 {
            need(data, p, 24, "GRAD")?;
            let mut t = [0.0f64; 6];
            for i in 0..6 { t[i] = get_f32(data, &mut p) as f64; }
            Some(t)
        } else { None };
        out.push(Gradient { kind, flags, stops, geometry, transform });
    }
    Ok(out)
}

// ---------- STYL ----------

#[derive(Debug, Default, Clone)]
pub struct Style {
    pub fill_kind: u8,
    pub fill_idx: u16,
    pub stroke_kind: u8,
    pub stroke_idx: u16,
    pub stroke_width: f32,
    pub cap: u8,
    pub join: u8,
    pub miter_limit: f32,
    pub dash: Vec<f32>,
    pub opacity: u8,
    pub blend_mode: u8,
}

pub fn pack_styl(styles: &[Style]) -> Vec<u8> {
    let mut v = Vec::new();
    put_u16(&mut v, styles.len() as u16);
    for s in styles {
        v.push(s.fill_kind);
        put_u16(&mut v, s.fill_idx);
        v.push(s.stroke_kind);
        put_u16(&mut v, s.stroke_idx);
        put_f32(&mut v, s.stroke_width);
        v.push(s.cap);
        v.push(s.join);
        put_f32(&mut v, s.miter_limit);
        v.push(s.dash.len() as u8);
        for d in &s.dash { put_f32(&mut v, *d); }
        v.push(s.opacity);
        v.push(s.blend_mode);
    }
    v
}

pub fn unpack_styl(data: &[u8]) -> Result<Vec<Style>, crate::BvgError> {
    need(data, 0, 2, "STYL")?;
    let mut p = 0usize;
    let n = get_u16(data, &mut p) as usize;
    let mut out = Vec::with_capacity(n);
    for _ in 0..n {
        need(data, p, 16, "STYL")?;
        let fill_kind = data[p]; p += 1;
        let fill_idx = get_u16(data, &mut p);
        let stroke_kind = data[p]; p += 1;
        let stroke_idx = get_u16(data, &mut p);
        let stroke_width = get_f32(data, &mut p);
        let cap = data[p]; p += 1;
        let join = data[p]; p += 1;
        let miter_limit = get_f32(data, &mut p);
        let dc = data[p] as usize; p += 1;
        need(data, p, dc * 4 + 2, "STYL")?;
        let mut dash = Vec::with_capacity(dc);
        for _ in 0..dc { dash.push(get_f32(data, &mut p)); }
        let opacity = data[p]; p += 1;
        let blend_mode = data[p]; p += 1;
        out.push(Style {
            fill_kind, fill_idx, stroke_kind, stroke_idx,
            stroke_width, cap, join, miter_limit, dash,
            opacity, blend_mode,
        });
    }
    Ok(out)
}

// ---------- NODE ----------

#[derive(Debug, Default, Clone)]
pub struct PathCmd {
    pub op: u8,
    pub args: Vec<f64>,
}

#[derive(Debug, Default, Clone)]
pub struct Node {
    pub type_: u8,
    pub flags: u8,
    pub style: u16,
    pub depth: u16,
    pub transform: Option<[f64; 6]>,

    pub child_count: u16,
    pub rect: [f64; 4],
    pub ellipse: [f64; 4],
    pub line: [f64; 4],
    pub points: Vec<[f64; 2]>,
    pub path: Vec<PathCmd>,
    pub font: u16,
    pub tx: f64,
    pub ty: f64,
    pub text: String,
    pub image_idx: u16,
    pub def_idx: u16,
}

impl Node {
    pub const GROUP: u8 = 0;
    pub const RECT: u8 = 1;
    pub const ELLIPSE: u8 = 2;
    pub const LINE: u8 = 3;
    pub const POLYLINE: u8 = 4;
    pub const POLYGON: u8 = 5;
    pub const PATH: u8 = 6;
    pub const TEXT: u8 = 7;
    pub const IMAGE: u8 = 8;
    pub const USE: u8 = 9;
    pub const DEF: u8 = 10;
    pub const HAS_TRANSFORM: u8 = 1;
}

pub fn pack_node(nodes: &[Node]) -> Vec<u8> {
    let mut v = Vec::new();
    for nd in nodes {
        let flags = if nd.transform.is_some() { Node::HAS_TRANSFORM } else { 0 };
        v.push(nd.type_);
        v.push(flags);
        put_u16(&mut v, nd.style);
        put_u16(&mut v, nd.depth);
        if let Some(t) = nd.transform {
            for x in t { put_f32(&mut v, x as f32); }
        }
        match nd.type_ {
            Node::GROUP | Node::DEF => {
                put_u16(&mut v, nd.child_count);
            }
            Node::RECT => {
                for x in nd.rect { put_f32(&mut v, x as f32); }
            }
            Node::ELLIPSE => {
                for x in nd.ellipse { put_f32(&mut v, x as f32); }
            }
            Node::LINE => {
                for x in nd.line { put_f32(&mut v, x as f32); }
            }
            Node::POLYLINE | Node::POLYGON => {
                put_u16(&mut v, nd.points.len() as u16);
                for p in &nd.points {
                    put_f32(&mut v, p[0] as f32);
                    put_f32(&mut v, p[1] as f32);
                }
            }
            Node::PATH => {
                put_u16(&mut v, nd.path.len() as u16);
                for c in &nd.path {
                    v.push(c.op);
                    if c.op == 4 {
                        if c.args.len() != 7 {
                            // fallback: serialize nothing to keep format valid
                            // but this is a programming error
                            continue;
                        }
                        put_f32(&mut v, c.args[0] as f32);
                        put_f32(&mut v, c.args[1] as f32);
                        put_f32(&mut v, c.args[2] as f32);
                        v.push(c.args[3] as u8);
                        v.push(c.args[4] as u8);
                        put_f32(&mut v, c.args[5] as f32);
                        put_f32(&mut v, c.args[6] as f32);
                    } else {
                        for a in &c.args { put_f32(&mut v, *a as f32); }
                    }
                }
            }
            Node::TEXT => {
                put_u16(&mut v, nd.font);
                put_f32(&mut v, nd.tx as f32);
                put_f32(&mut v, nd.ty as f32);
                let s = nd.text.as_bytes();
                put_u16(&mut v, s.len() as u16);
                v.extend_from_slice(s);
            }
            Node::IMAGE => {
                put_u16(&mut v, nd.image_idx);
                for x in nd.rect { put_f32(&mut v, x as f32); }
            }
            Node::USE => {
                put_u16(&mut v, nd.def_idx);
                put_f32(&mut v, nd.tx as f32);
                put_f32(&mut v, nd.ty as f32);
            }
            _ => {}
        }
    }
    v
}

pub fn unpack_node(data: &[u8]) -> Result<Vec<Node>, crate::BvgError> {
    let mut out = Vec::new();
    let mut p = 0usize;
    while p < data.len() {
        need(data, p, 6, "NODE")?;
        let mut nd = Node::default();
        nd.type_ = data[p]; p += 1;
        nd.flags = data[p]; p += 1;
        nd.style = get_u16(data, &mut p);
        nd.depth = get_u16(data, &mut p);
        if nd.flags & Node::HAS_TRANSFORM != 0 {
            need(data, p, 24, "NODE")?;
            let mut t = [0.0f64; 6];
            for i in 0..6 { t[i] = get_f32(data, &mut p) as f64; }
            nd.transform = Some(t);
        }
        match nd.type_ {
            Node::GROUP | Node::DEF => {
                need(data, p, 2, "NODE")?;
                nd.child_count = get_u16(data, &mut p);
            }
            Node::RECT => {
                need(data, p, 16, "NODE")?;
                for i in 0..4 { nd.rect[i] = get_f32(data, &mut p) as f64; }
            }
            Node::ELLIPSE => {
                need(data, p, 16, "NODE")?;
                for i in 0..4 { nd.ellipse[i] = get_f32(data, &mut p) as f64; }
            }
            Node::LINE => {
                need(data, p, 16, "NODE")?;
                for i in 0..4 { nd.line[i] = get_f32(data, &mut p) as f64; }
            }
            Node::POLYLINE | Node::POLYGON => {
                need(data, p, 2, "NODE")?;
                let n = get_u16(data, &mut p) as usize;
                need(data, p, n * 8, "NODE")?;
                nd.points.reserve(n);
                for _ in 0..n {
                    let x = get_f32(data, &mut p) as f64;
                    let y = get_f32(data, &mut p) as f64;
                    nd.points.push([x, y]);
                }
            }
            Node::PATH => {
                need(data, p, 2, "NODE")?;
                let n = get_u16(data, &mut p) as usize;
                nd.path.reserve(n);
                for _ in 0..n {
                    need(data, p, 1, "NODE")?;
                    let op = data[p]; p += 1;
                    let args: Vec<f64> = if op == 4 {
                        need(data, p, 22, "NODE")?;
                        let rx = get_f32(data, &mut p) as f64;
                        let ry = get_f32(data, &mut p) as f64;
                        let rot = get_f32(data, &mut p) as f64;
                        let large_arc = data[p] as f64; p += 1;
                        let sweep = data[p] as f64; p += 1;
                        let x = get_f32(data, &mut p) as f64;
                        let y = get_f32(data, &mut p) as f64;
                        vec![rx, ry, rot, large_arc, sweep, x, y]
                    } else {
                        let argc = match op {
                            0 | 1 => 2, 2 => 4, 3 => 6, _ => 0,
                        };
                        need(data, p, argc * 4, "NODE")?;
                        let mut a = Vec::with_capacity(argc);
                        for _ in 0..argc { a.push(get_f32(data, &mut p) as f64); }
                        a
                    };
                    nd.path.push(PathCmd { op, args });
                }
            }
            Node::TEXT => {
                need(data, p, 12, "NODE")?;
                nd.font = get_u16(data, &mut p);
                nd.tx = get_f32(data, &mut p) as f64;
                nd.ty = get_f32(data, &mut p) as f64;
                let len = get_u16(data, &mut p) as usize;
                need(data, p, len, "NODE")?;
                nd.text = String::from_utf8_lossy(&data[p..p+len]).to_string();
                p += len;
            }
            Node::IMAGE => {
                need(data, p, 18, "NODE")?;
                nd.image_idx = get_u16(data, &mut p);
                for i in 0..4 { nd.rect[i] = get_f32(data, &mut p) as f64; }
            }
            Node::USE => {
                need(data, p, 10, "NODE")?;
                nd.def_idx = get_u16(data, &mut p);
                nd.tx = get_f32(data, &mut p) as f64;
                nd.ty = get_f32(data, &mut p) as f64;
            }
            _ => return Err(crate::BvgError::chunk("NODE: bad type")),
        }
        out.push(nd);
    }
    Ok(out)
}

// ---------- IMGS ----------

#[derive(Debug, Default, Clone)]
pub struct Image {
    pub format: u8,
    pub width: u16,
    pub height: u16,
    pub data: Vec<u8>,
}

pub fn unpack_images(data: &[u8]) -> Result<Vec<Image>, crate::BvgError> {
    need(data, 0, 2, "IMGS")?;
    let mut p = 0usize;
    let n = get_u16(data, &mut p) as usize;
    let mut out = Vec::with_capacity(n);
    for _ in 0..n {
        need(data, p, 9, "IMGS")?;
        let format = data[p]; p += 1;
        let width = get_u16(data, &mut p);
        let height = get_u16(data, &mut p);
        let dlen = get_u32(data, &mut p) as usize;
        need(data, p, dlen, "IMGS")?;
        let data_bytes = data[p..p+dlen].to_vec();
        p += dlen;
        out.push(Image { format, width, height, data: data_bytes });
    }
    Ok(out)
}