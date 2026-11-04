use std::collections::HashMap;
use std::path::Path;
use crate::file::BvgFile;
use crate::generated::*;
use crate::BvgError;

pub struct Document {
    pub width: u32,
    pub height: u32,
    palette: Vec<[u8; 4]>,
    color_idx: HashMap<[u8; 4], u16>,
    gradients: Vec<Gradient>,
    styles: Vec<Style>,
    nodes: Vec<Node>,
}

impl Document {
    pub fn new(w: u32, h: u32) -> Self {
        Self { width: w, height: h,
               palette: Vec::new(), color_idx: HashMap::new(),
               gradients: Vec::new(), styles: Vec::new(),
               nodes: Vec::new() }
    }

    fn color(&mut self, c: [u8; 4]) -> u16 {
        if let Some(&i) = self.color_idx.get(&c) { return i; }
        let i = self.palette.len() as u16;
        self.palette.push(c);
        self.color_idx.insert(c, i);
        i
    }

    fn style_solid(&mut self, ci: u16) -> u16 {
        let s = Style {
            fill_kind: 1, fill_idx: ci,
            stroke_kind: 0, stroke_idx: 0,
            stroke_width: 0.0, cap: 0, join: 0,
            miter_limit: 4.0, dash: Vec::new(),
            opacity: 255, blend_mode: 0,
        };
        self.styles.push(s);
        (self.styles.len() - 1) as u16
    }

    pub fn rect(&mut self, x: f64, y: f64, w: f64, h: f64, fill: &str) -> &mut Self {
        let v = parse_hex(fill);
        let ci = self.color([v[0], v[1], v[2], 255]);
        let si = self.style_solid(ci);
        self.nodes.push(Node {
            type_: Node::RECT,
            rect: [x, y, w, h],
            style: si,
            ..Default::default()
        });
        self
    }

    pub fn to_bvgfile(&self) -> BvgFile {
        let mut f = BvgFile::new(self.width, self.height);
        if !self.palette.is_empty() {
            f.set_chunk("COLR", pack_colr(&self.palette), 0, 0, false);
        }
        if !self.gradients.is_empty() {
            f.set_chunk("GRAD", pack_grad(&self.gradients), 0, 0, false);
        }
        if !self.styles.is_empty() {
            f.set_chunk("STYL", pack_styl(&self.styles), 0, 0, false);
        }
        // SPEC 3.2: NODE is always present (possibly empty)
        f.set_chunk("NODE", pack_node(&self.nodes), 0, 0, false);
        f
    }

    pub fn save<P: AsRef<Path>>(&self, p: P, codec: u8, level: u8)
        -> Result<(), BvgError>
    {
        let f = self.to_bvgfile();
        f.write(p, true, true, codec, level)
    }
}

fn parse_hex(s: &str) -> [u8; 3] {
    let h = s.trim_start_matches('#');
    let h = if h.len() == 3 {
        h.chars().flat_map(|c| [c, c]).collect::<String>()
    } else { h.to_string() };
    let r = u8::from_str_radix(&h[0..2], 16).unwrap_or(0);
    let g = u8::from_str_radix(&h[2..4], 16).unwrap_or(0);
    let b = u8::from_str_radix(&h[4..6], 16).unwrap_or(0);
    [r, g, b]
}