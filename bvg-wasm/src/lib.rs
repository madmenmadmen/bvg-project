use wasm_bindgen::prelude::*;
use wasm_bindgen::JsCast;
use wasm_bindgen_futures::JsFuture;
use web_sys::{
    CanvasRenderingContext2d, CanvasGradient, HtmlCanvasElement, ImageBitmap,
    ImageData,
};

use bvg::file::BvgFile;
use bvg::generated::*;

#[wasm_bindgen]
pub struct WasmBvg {
    width: u32,
    height: u32,
    chunk_names: String,
    palette: Vec<[u8; 4]>,
    grads: Vec<Gradient>,
    styles: Vec<Style>,
    nodes: Vec<Node>,
    images: Vec<Option<HtmlCanvasElement>>,  // raw RGBA -> offscreen canvas
    bitmaps: Vec<Option<ImageBitmap>>,       // PNG / JPEG -> ImageBitmap
    imgs_meta: Vec<(u8, u16, u16, Vec<u8>)>,
}

#[wasm_bindgen]
impl WasmBvg {
    #[wasm_bindgen(constructor)]
    pub fn new(data: &[u8]) -> Result<WasmBvg, JsValue> {
        console_error_panic_hook::set_once();
        let f = BvgFile::read_from_bytes(data)
            .map_err(|e| JsValue::from_str(&format!("read: {e}")))?;

        let palette = f.get_chunk("COLR")
            .map(|r| unpack_colr(r))
            .transpose()
            .map_err(|e| JsValue::from_str(&format!("COLR: {e}")))?
            .unwrap_or_default();
        let grads = f.get_chunk("GRAD")
            .map(|r| unpack_grad(r))
            .transpose()
            .map_err(|e| JsValue::from_str(&format!("GRAD: {e}")))?
            .unwrap_or_default();
        let styles = f.get_chunk("STYL")
            .map(|r| unpack_styl(r))
            .transpose()
            .map_err(|e| JsValue::from_str(&format!("STYL: {e}")))?
            .unwrap_or_default();
        let nodes = f.get_chunk("NODE")
            .map(|r| unpack_node(r))
            .transpose()
            .map_err(|e| JsValue::from_str(&format!("NODE: {e}")))?
            .unwrap_or_default();

        let mut imgs_meta = Vec::new();
        if let Some(raw) = f.get_chunk("IMGS") {
            let imgs = unpack_images(raw)
                .map_err(|e| JsValue::from_str(&format!("IMGS: {e}")))?;
            for im in imgs {
                imgs_meta.push((im.format, im.width, im.height, im.data));
            }
        }

        let chunk_names = f.order.join(",");
        let n_images = imgs_meta.len();

        Ok(WasmBvg {
            width: f.width,
            height: f.height,
            chunk_names,
            palette,
            grads,
            styles,
            nodes,
            images: vec![None; n_images],
            bitmaps: vec![None; n_images],
            imgs_meta,
        })
    }

    #[wasm_bindgen(getter)]
    pub fn width(&self) -> u32 { self.width }
    #[wasm_bindgen(getter)]
    pub fn height(&self) -> u32 { self.height }
    #[wasm_bindgen(getter)]
    pub fn chunks(&self) -> String { self.chunk_names.clone() }
    #[wasm_bindgen(getter)]
    pub fn node_count(&self) -> usize { self.nodes.len() }

    pub async fn load_images(&mut self) -> Result<(), JsValue> {
        let win = web_sys::window().ok_or("no window")?;
        let doc = win.document().ok_or("no document")?;
        for (i, (fmt, w, h, data)) in self.imgs_meta.iter().enumerate() {
            match *fmt {
                0 | 1 => {
                    // PNG / JPEG: Blob -> ImageBitmap
                    let b64 = base64_encode(data);
                    let mime = if *fmt == 0 { "image/png" } else { "image/jpeg" };
                    let uri = format!("data:{};base64,{}", mime, b64);
                    let resp = JsFuture::from(win.fetch_with_str(&uri)).await?;
                    let resp: web_sys::Response = resp.dyn_into()?;
                    let blob = JsFuture::from(resp.blob()?).await?;
                    let blob: web_sys::Blob = blob.dyn_into()?;
                    let promise = win.create_image_bitmap_with_blob(&blob)?;
                    let bitmap: ImageBitmap = JsFuture::from(promise).await?.dyn_into()?;
                    self.bitmaps[i] = Some(bitmap);
                }
                2 => {
                    // raw RGBA -> offscreen canvas with putImageData
                    let imagedata = ImageData::new_with_u8_clamped_array_and_sh(
                        wasm_bindgen::Clamped(data.as_slice()),
                        *w as u32,
                        *h as u32,
                    )?;
                    let canvas = doc.create_element("canvas")?
                        .dyn_into::<HtmlCanvasElement>()?;
                    canvas.set_width(*w as u32);
                    canvas.set_height(*h as u32);
                    let ctx: CanvasRenderingContext2d = canvas
                        .get_context("2d")?
                        .ok_or("no 2d context")?
                        .dyn_into()?;
                    ctx.put_image_data(&imagedata, 0.0, 0.0)?;
                    self.images[i] = Some(canvas);
                }
                _ => return Err(JsValue::from_str("bad image format")),
            }
        }
        Ok(())
    }

    #[wasm_bindgen]
    pub fn render(&self, canvas_id: &str) -> Result<(), JsValue> {
        let win = web_sys::window().ok_or("no window")?;
        let doc = win.document().ok_or("no document")?;
        let el = doc.get_element_by_id(canvas_id)
            .ok_or("no canvas element")?;
        let canvas: HtmlCanvasElement = el.dyn_into()?;
        canvas.set_width(self.width);
        canvas.set_height(self.height);
        let ctx: CanvasRenderingContext2d = canvas
            .get_context("2d")?
            .ok_or("no 2d context")?
            .dyn_into()?;

        ctx.set_fill_style_str("rgba(0,0,0,0)");
        ctx.clear_rect(0.0, 0.0, self.width as f64, self.height as f64);

        self.draw_range(&ctx, 0, self.nodes.len())?;
        Ok(())
    }

    fn color_str(&self, idx: u16) -> String {
        if idx == 0xFFFF || (idx as usize) >= self.palette.len() {
            return "rgba(0,0,0,0)".to_string();
        }
        let c = self.palette[idx as usize];
        format!("rgba({},{},{},{})",
                c[0], c[1], c[2], (c[3] as f32) / 255.0)
    }

    fn make_brush(&self, ctx: &CanvasRenderingContext2d, gi: u16)
        -> Result<Option<JsValue>, JsValue>
    {
        if (gi as usize) >= self.grads.len() {
            return Ok(None);
        }
        let g = &self.grads[gi as usize];
        match g.kind {
            GRAD_LINEAR => {
                if g.geometry.len() < 4 { return Ok(None); }
                let grad = ctx.create_linear_gradient(
                    g.geometry[0], g.geometry[1],
                    g.geometry[2], g.geometry[3]);
                self.add_stops(&grad, g)?;
                Ok(Some(grad.into()))
            }
            GRAD_RADIAL => {
                if g.geometry.len() < 5 { return Ok(None); }
                let grad = ctx.create_radial_gradient(
                    g.geometry[3], g.geometry[4], 0.0,
                    g.geometry[0], g.geometry[1], g.geometry[2])?;
                self.add_stops(&grad, g)?;
                Ok(Some(grad.into()))
            }
            _ => Ok(None),
        }
    }

    fn add_stops(&self, grad: &CanvasGradient, g: &Gradient)
        -> Result<(), JsValue>
    {
        for st in &g.stops {
            let col = if st.color_idx == 0xFFFF {
                format!("rgba({},{},{},{})",
                        st.rgba[0], st.rgba[1], st.rgba[2],
                        (st.rgba[3] as f32) / 255.0)
            } else {
                self.color_str(st.color_idx)
            };
            grad.add_color_stop(st.offset, &col)?;
        }
        Ok(())
    }

    fn set_fill(&self, ctx: &CanvasRenderingContext2d, s: &Style)
        -> Result<(), JsValue>
    {
        match s.fill_kind {
            0 => ctx.set_fill_style_str("rgba(0,0,0,0)"),
            1 => ctx.set_fill_style_str(&self.color_str(s.fill_idx)),
            2 => {
                if let Some(brush) = self.make_brush(ctx, s.fill_idx)? {
                    let grad = brush.dyn_into::<CanvasGradient>()?;
                    ctx.set_fill_style_canvas_gradient(&grad);
                }
            }
            _ => {}
        }
        Ok(())
    }

    fn set_stroke(&self, ctx: &CanvasRenderingContext2d, s: &Style)
        -> Result<(), JsValue>
    {
        match s.stroke_kind {
            0 => ctx.set_stroke_style_str("rgba(0,0,0,0)"),
            1 => ctx.set_stroke_style_str(&self.color_str(s.stroke_idx)),
            2 => {
                if let Some(brush) = self.make_brush(ctx, s.stroke_idx)? {
                    let grad = brush.dyn_into::<CanvasGradient>()?;
                    ctx.set_stroke_style_canvas_gradient(&grad);
                }
            }
            _ => {}
        }
        ctx.set_line_width(s.stroke_width as f64);
        ctx.set_line_cap(match s.cap {
            0 => "butt", 1 => "round", 2 => "square", _ => "butt" });
        ctx.set_line_join(match s.join {
            0 => "miter", 1 => "round", 2 => "bevel", _ => "miter" });
        ctx.set_miter_limit(s.miter_limit as f64);
        if !s.dash.is_empty() {
            let dash: js_sys::Array = s.dash.iter()
                .map(|d| JsValue::from_f64(*d as f64))
                .collect();
            ctx.set_line_dash(&dash)?;
        } else {
            ctx.set_line_dash(&js_sys::Array::new())?;
        }
        Ok(())
    }

    fn apply_blend(&self, ctx: &CanvasRenderingContext2d, bm: u8) {
        let op = match bm {
            1 => "multiply",
            2 => "screen",
            3 => "overlay",
            _ => "source-over",
        };
        let _ = ctx.set_global_composite_operation(op);
    }

    fn draw_range(&self, ctx: &CanvasRenderingContext2d,
                  start: usize, end: usize) -> Result<(), JsValue>
    {
        let mut i = start;
        while i < end {
            let nd = &self.nodes[i];
            match nd.type_ {
                Node::DEF => {
                    i += 1 + nd.child_count as usize;
                    continue;
                }
                _ => {}
            }
            let style = if (nd.style as usize) < self.styles.len() {
                Some(&self.styles[nd.style as usize])
            } else {
                None
            };

            ctx.save();
            if let Some(t) = nd.transform {
                ctx.transform(t[0], t[1], t[2], t[3], t[4], t[5])?;
            }
            if let Some(s) = style {
                self.apply_blend(ctx, s.blend_mode);
                if s.opacity < 255 {
                    ctx.set_global_alpha((s.opacity as f64) / 255.0);
                }
                self.set_fill(ctx, s)?;
                self.set_stroke(ctx, s)?;
            }

            match nd.type_ {
                Node::GROUP => {
                    let cc = nd.child_count as usize;
                    self.draw_range(ctx, i + 1, i + 1 + cc)?;
                    i += 1 + cc;
                    ctx.restore();
                    continue;
                }
                Node::RECT => {
                    ctx.fill_rect(nd.rect[0], nd.rect[1],
                                  nd.rect[2], nd.rect[3]);
                    if let Some(s) = style {
                        if s.stroke_kind != 0 {
                            ctx.stroke_rect(nd.rect[0], nd.rect[1],
                                            nd.rect[2], nd.rect[3]);
                        }
                    }
                }
                Node::ELLIPSE => {
                    ctx.begin_path();
                    ctx.ellipse(nd.ellipse[0], nd.ellipse[1],
                                nd.ellipse[2], nd.ellipse[3],
                                0.0, 0.0, std::f64::consts::TAU)?;
                    ctx.fill();
                    if let Some(s) = style {
                        if s.stroke_kind != 0 { ctx.stroke(); }
                    }
                }
                Node::LINE => {
                    ctx.begin_path();
                    ctx.move_to(nd.line[0], nd.line[1]);
                    ctx.line_to(nd.line[2], nd.line[3]);
                    ctx.stroke();
                }
                Node::POLYLINE | Node::POLYGON => {
                    ctx.begin_path();
                    let pts = &nd.points;
                    if !pts.is_empty() {
                        ctx.move_to(pts[0][0], pts[0][1]);
                        for p in &pts[1..] {
                            ctx.line_to(p[0], p[1]);
                        }
                        if nd.type_ == Node::POLYGON { ctx.close_path(); }
                    }
                    ctx.fill();
                    if let Some(s) = style {
                        if s.stroke_kind != 0 { ctx.stroke(); }
                    }
                }
                Node::PATH => {
                    ctx.begin_path();
                    self.build_path(ctx, &nd.path)?;
                    ctx.fill();
                    if let Some(s) = style {
                        if s.stroke_kind != 0 { ctx.stroke(); }
                    }
                }
                Node::TEXT => {
                    ctx.set_font(&format!("{}px sans-serif",
                        style.map(|s| s.stroke_width).unwrap_or(12.0)));
                    ctx.fill_text(&nd.text, nd.tx, nd.ty)?;
                }
                Node::IMAGE => {
                    let idx = nd.image_idx as usize;
                    // raw RGBA -> offscreen canvas
                    if let Some(canvas) = self.images.get(idx).and_then(|o| o.as_ref()) {
                        ctx.draw_image_with_html_canvas_element_and_dw_and_dh(
                            canvas,
                            nd.rect[0], nd.rect[1],
                            nd.rect[2], nd.rect[3],
                        )?;
                    }
                    // PNG/JPEG -> ImageBitmap
                    if let Some(bitmap) = self.bitmaps.get(idx).and_then(|o| o.as_ref()) {
                        ctx.draw_image_with_image_bitmap_and_dw_and_dh(
                            bitmap,
                            nd.rect[0], nd.rect[1],
                            nd.rect[2], nd.rect[3],
                        )?;
                    }
                }
                Node::USE => {
                    let def_idx = nd.def_idx as usize;
                    let mut di = 0;
                    let mut def_start = None;
                    for j in 0..self.nodes.len() {
                        if self.nodes[j].type_ == Node::DEF {
                            if di == def_idx { def_start = Some(j); break; }
                            di += 1;
                        }
                    }
                    if let Some(s) = def_start {
                        let cc = self.nodes[s].child_count as usize;
                        ctx.save();
                        ctx.translate(nd.tx, nd.ty)?;
                        self.draw_range(ctx, s + 1, s + 1 + cc)?;
                        ctx.restore();
                    }
                }
                _ => {}
            }
            ctx.restore();
            i += 1;
        }
        Ok(())
    }

    fn build_path(&self, ctx: &CanvasRenderingContext2d,
                  cmds: &[PathCmd]) -> Result<(), JsValue>
    {
        let mut cur_x = 0.0f64;
        let mut cur_y = 0.0f64;
        for c in cmds {
            match c.op {
                0 => { ctx.move_to(c.args[0], c.args[1]);
                       cur_x = c.args[0]; cur_y = c.args[1]; }
                1 => { ctx.line_to(c.args[0], c.args[1]);
                       cur_x = c.args[0]; cur_y = c.args[1]; }
                2 => { ctx.quadratic_curve_to(c.args[0], c.args[1],
                                              c.args[2], c.args[3]);
                       cur_x = c.args[2]; cur_y = c.args[3]; }
                3 => { ctx.bezier_curve_to(c.args[0], c.args[1],
                                           c.args[2], c.args[3],
                                           c.args[4], c.args[5]);
                       cur_x = c.args[4]; cur_y = c.args[5]; }
                4 => {
                    let rx = c.args[0]; let ry = c.args[1];
                    let rot = c.args[2]; let la = c.args[3];
                    let sw = c.args[4]; let x = c.args[5]; let y = c.args[6];
                    self.svg_arc(ctx, cur_x, cur_y, rx, ry, rot, la, sw, x, y)?;
                    cur_x = x; cur_y = y;
                }
                5 => ctx.close_path(),
                _ => {}
            }
        }
        Ok(())
    }

    fn svg_arc(&self, ctx: &CanvasRenderingContext2d,
               x1: f64, y1: f64, rx: f64, ry: f64, rot: f64,
               large_arc: f64, sweep: f64, x2: f64, y2: f64)
        -> Result<(), JsValue>
    {
        if rx == 0.0 || ry == 0.0 {
            ctx.line_to(x2, y2);
            return Ok(());
        }
        let rx = rx.abs(); let ry = ry.abs();
        let cos_phi = rot.cos(); let sin_phi = rot.sin();

        let dx = (x1 - x2) / 2.0;
        let dy = (y1 - y2) / 2.0;
        let x1p =  cos_phi * dx + sin_phi * dy;
        let y1p = -sin_phi * dx + cos_phi * dy;

        let lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
        let (rx, ry) = if lam > 1.0 {
            let s = lam.sqrt();
            (rx * s, ry * s)
        } else {
            (rx, ry)
        };

        let num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
        let den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
        if den == 0.0 {
            ctx.line_to(x2, y2);
            return Ok(());
        }
        let coef = (num / den).max(0.0).sqrt();
        let coef = if large_arc == sweep { -coef } else { coef };
        let cxp =  coef * (rx * y1p) / ry;
        let cyp = -coef * (ry * x1p) / rx;

        let cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2.0;
        let cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2.0;

        let angle = |ux: f64, uy: f64, vx: f64, vy: f64| -> f64 {
            let dot = ux * vx + uy * vy;
            let lu = (ux * ux + uy * uy).sqrt();
            let lv = (vx * vx + vy * vy).sqrt();
            if lu == 0.0 || lv == 0.0 { return 0.0; }
            let c = (dot / (lu * lv)).clamp(-1.0, 1.0);
            let mut a = c.acos();
            if ux * vy - uy * vx < 0.0 { a = -a; }
            a
        };

        let theta1 = angle(1.0, 0.0, (x1p - cxp) / rx, (y1p - cyp) / ry);
        let mut dtheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry,
                               (-x1p - cxp) / rx, (-y1p - cyp) / ry);
        if !sw_bool(sweep) && dtheta > 0.0 { dtheta -= 2.0 * std::f64::consts::PI; }
        if  sw_bool(sweep) && dtheta < 0.0 { dtheta += 2.0 * std::f64::consts::PI; }

        let n = ((dtheta.abs() * 64.0).ceil() as usize).max(16);
        for k in 1..=n {
            let t = theta1 + dtheta * (k as f64) / (n as f64);
            let ex = rx * t.cos();
            let ey = ry * t.sin();
            let px = cos_phi * ex - sin_phi * ey + cx;
            let py = sin_phi * ex + cos_phi * ey + cy;
            ctx.line_to(px, py);
        }
        ctx.line_to(x2, y2);
        Ok(())
    }
}

fn sw_bool(v: f64) -> bool { v != 0.0 }

fn base64_encode(data: &[u8]) -> String {
    const T: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut out = String::new();
    let mut i = 0;
    while i + 2 < data.len() {
        let n = ((data[i] as u32) << 16)
              | ((data[i+1] as u32) << 8)
              |  (data[i+2] as u32);
        out.push(T[((n >> 18) & 63) as usize] as char);
        out.push(T[((n >> 12) & 63) as usize] as char);
        out.push(T[((n >> 6) & 63) as usize] as char);
        out.push(T[(n & 63) as usize] as char);
        i += 3;
    }
    if i + 1 == data.len() {
        let n = (data[i] as u32) << 16;
        out.push(T[((n >> 18) & 63) as usize] as char);
        out.push(T[((n >> 12) & 63) as usize] as char);
        out.push('=');
        out.push('=');
    } else if i + 2 == data.len() {
        let n = ((data[i] as u32) << 16)
              | ((data[i+1] as u32) << 8);
        out.push(T[((n >> 18) & 63) as usize] as char);
        out.push(T[((n >> 12) & 63) as usize] as char);
        out.push(T[((n >> 6) & 63) as usize] as char);
        out.push('=');
    }
    out
}

#[wasm_bindgen]
pub fn bvg_info(data: &[u8]) -> Result<String, JsValue> {
    let f = BvgFile::read_from_bytes(data)
        .map_err(|e| JsValue::from_str(&format!("read: {e}")))?;
    Ok(format!("{}x{} chunks={}",
               f.width, f.height, f.order.join(",")))
}

#[wasm_bindgen]
pub fn bvg_dump(data: &[u8]) -> Result<String, JsValue> {
    let f = BvgFile::read_from_bytes(data)
        .map_err(|e| JsValue::from_str(&format!("read: {e}")))?;
    let mut js = String::new();
    js.push_str("{\n  \"chunks\": [\n");
    let mut first = true;
    for t in &f.order {
        let raw = &f.chunks[t];
        let m = f.meta.get(t);
        let codec = m.map(|m| m.codec).unwrap_or(0);
        let level = m.map(|m| m.level).unwrap_or(0);
        let use_pre = m.map(|m| m.use_pre).unwrap_or(false);
        if !first { js.push_str(",\n"); }
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
    js.push_str("  \"file\": \"\",\n");
    js.push_str(&format!("  \"height\": {},\n", f.height));
    js.push_str(&format!("  \"width\": {}\n", f.width));
    js.push_str("}\n");
    Ok(js)
}

fn hex(data: &[u8]) -> String {
    let mut s = String::with_capacity(data.len() * 2);
    for b in data {
        s.push_str(&format!("{:02x}", b));
    }
    s
}