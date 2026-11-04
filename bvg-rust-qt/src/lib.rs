use std::ffi::CStr;
use std::os::raw::{c_char, c_int};

use bvg::file::BvgFile;
use bvg::generated::*;

pub struct BvgHandle {
    file: BvgFile,
    palette: Vec<[u8; 4]>,
    grads: Vec<Gradient>,
    styles: Vec<Style>,
    nodes: Vec<Node>,
    images: Vec<Image>,
}

unsafe fn node<'a>(h: *const BvgHandle, idx: usize) -> Option<&'a Node> {
    if h.is_null() { return None; }
    let hh = &*h;
    hh.nodes.get(idx)
}

// ---------- open/free ----------

#[no_mangle]
pub extern "C" fn bvg_open(path: *const c_char) -> *mut BvgHandle {
    if path.is_null() { return std::ptr::null_mut(); }
    let cs = unsafe { CStr::from_ptr(path) };
    let s = match cs.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    match BvgFile::read(s) {
        Ok(f) => {
            let palette = f.get_chunk("COLR")
                .and_then(|r| unpack_colr(r).ok())
                .unwrap_or_default();
            let grads = f.get_chunk("GRAD")
                .and_then(|r| unpack_grad(r).ok())
                .unwrap_or_default();
            let styles = f.get_chunk("STYL")
                .and_then(|r| unpack_styl(r).ok())
                .unwrap_or_default();
            let nodes = f.get_chunk("NODE")
                .and_then(|r| unpack_node(r).ok())
                .unwrap_or_default();
            let images = f.get_chunk("IMGS")
                .and_then(|r| unpack_images(r).ok())
                .unwrap_or_default();
            Box::into_raw(Box::new(BvgHandle { file: f, palette, grads, styles, nodes, images }))
        }
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn bvg_free(h: *mut BvgHandle) {
    if !h.is_null() {
        unsafe { drop(Box::from_raw(h)); }
    }
}

// ---------- header ----------

#[no_mangle]
pub extern "C" fn bvg_width(h: *const BvgHandle) -> c_int {
    if h.is_null() { return 0; }
    unsafe { (*h).file.width as c_int }
}

#[no_mangle]
pub extern "C" fn bvg_height(h: *const BvgHandle) -> c_int {
    if h.is_null() { return 0; }
    unsafe { (*h).file.height as c_int }
}

// ---------- nodes ----------

#[no_mangle]
pub extern "C" fn bvg_node_count(h: *const BvgHandle) -> usize {
    if h.is_null() { return 0; }
    unsafe { (*h).nodes.len() }
}

#[no_mangle]
pub extern "C" fn bvg_node_type(h: *const BvgHandle, idx: usize) -> c_int {
    match unsafe { node(h, idx) } {
        Some(n) => n.type_ as c_int,
        None => -1,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_style(h: *const BvgHandle, idx: usize) -> u16 {
    match unsafe { node(h, idx) } {
        Some(n) => n.style,
        None => 0xFFFF,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_depth(h: *const BvgHandle, idx: usize) -> u16 {
    match unsafe { node(h, idx) } {
        Some(n) => n.depth,
        None => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_child_count(h: *const BvgHandle, idx: usize) -> u16 {
    match unsafe { node(h, idx) } {
        Some(n) => n.child_count,
        None => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_has_transform(h: *const BvgHandle, idx: usize) -> c_int {
    match unsafe { node(h, idx) } {
        Some(n) => if n.transform.is_some() { 1 } else { 0 },
        None => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_transform(h: *const BvgHandle, idx: usize, out6: *mut f64) -> c_int {
    if out6.is_null() { return 0; }
    match unsafe { node(h, idx) } {
        Some(n) => match n.transform {
            Some(t) => {
                unsafe { for i in 0..6 { *out6.add(i) = t[i]; } }
                1
            }
            None => 0,
        },
        None => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_rect(h: *const BvgHandle, idx: usize, out4: *mut f64) -> c_int {
    if out4.is_null() { return 0; }
    match unsafe { node(h, idx) } {
        Some(n) if n.type_ == Node::RECT || n.type_ == Node::IMAGE => {
            unsafe { for i in 0..4 { *out4.add(i) = n.rect[i]; } }
            1
        }
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_ellipse(h: *const BvgHandle, idx: usize, out4: *mut f64) -> c_int {
    if out4.is_null() { return 0; }
    match unsafe { node(h, idx) } {
        Some(n) if n.type_ == Node::ELLIPSE => {
            unsafe { for i in 0..4 { *out4.add(i) = n.ellipse[i]; } }
            1
        }
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_line(h: *const BvgHandle, idx: usize, out4: *mut f64) -> c_int {
    if out4.is_null() { return 0; }
    match unsafe { node(h, idx) } {
        Some(n) if n.type_ == Node::LINE => {
            unsafe { for i in 0..4 { *out4.add(i) = n.line[i]; } }
            1
        }
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_point_count(h: *const BvgHandle, idx: usize) -> usize {
    match unsafe { node(h, idx) } {
        Some(n) if n.type_ == Node::POLYLINE || n.type_ == Node::POLYGON => n.points.len(),
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_points(h: *const BvgHandle, idx: usize, out2n: *mut f64) -> c_int {
    if out2n.is_null() { return 0; }
    match unsafe { node(h, idx) } {
        Some(n) if n.type_ == Node::POLYLINE || n.type_ == Node::POLYGON => {
            for (i, p) in n.points.iter().enumerate() {
                unsafe {
                    *out2n.add(i * 2) = p[0];
                    *out2n.add(i * 2 + 1) = p[1];
                }
            }
            1
        }
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_path_count(h: *const BvgHandle, idx: usize) -> usize {
    match unsafe { node(h, idx) } {
        Some(n) if n.type_ == Node::PATH => n.path.len(),
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_path(h: *const BvgHandle, idx: usize,
                                out_ops: *mut u8, out_args: *mut f64) -> c_int {
    if out_ops.is_null() || out_args.is_null() { return 0; }
    match unsafe { node(h, idx) } {
        Some(n) if n.type_ == Node::PATH => {
            for (i, c) in n.path.iter().enumerate() {
                unsafe {
                    *out_ops.add(i) = c.op;
                    for j in 0..7 {
                        *out_args.add(i * 8 + j) = if j < c.args.len() { c.args[j] } else { 0.0 };
                    }
                }
            }
            1
        }
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_text(h: *const BvgHandle, idx: usize,
                                out_xy: *mut f64, out_buf: *mut u8,
                                buf_len: usize) -> c_int {
    if out_xy.is_null() || out_buf.is_null() { return 0; }
    match unsafe { node(h, idx) } {
        Some(n) if n.type_ == Node::TEXT => {
            unsafe {
                *out_xy.add(0) = n.tx;
                *out_xy.add(1) = n.ty;
            }
            let bytes = n.text.as_bytes();
            let n_copy = bytes.len().min(buf_len);
            unsafe {
                std::ptr::copy_nonoverlapping(bytes.as_ptr(), out_buf, n_copy);
            }
            n_copy as c_int
        }
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_image(h: *const BvgHandle, idx: usize, out_img: *mut u16) -> c_int {
    if out_img.is_null() { return 0; }
    match unsafe { node(h, idx) } {
        Some(n) if n.type_ == Node::IMAGE => {
            unsafe { *out_img = n.image_idx; }
            1
        }
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn bvg_node_use(h: *const BvgHandle, idx: usize,
                               out_def: *mut u16, out_xy: *mut f64) -> c_int {
    if out_def.is_null() || out_xy.is_null() { return 0; }
    match unsafe { node(h, idx) } {
        Some(n) if n.type_ == Node::USE => {
            unsafe {
                *out_def = n.def_idx;
                *out_xy.add(0) = n.tx;
                *out_xy.add(1) = n.ty;
            }
            1
        }
        _ => 0,
    }
}

// ---------- palette ----------

#[no_mangle]
pub extern "C" fn bvg_palette_size(h: *const BvgHandle) -> usize {
    if h.is_null() { return 0; }
    unsafe { (*h).palette.len() }
}

#[no_mangle]
pub extern "C" fn bvg_palette_get(h: *const BvgHandle, idx: usize, out_rgba: *mut u8) -> c_int {
    if out_rgba.is_null() || h.is_null() { return 0; }
    let hh = unsafe { &*h };
    if idx >= hh.palette.len() { return 0; }
    let c = hh.palette[idx];
    unsafe { for i in 0..4 { *out_rgba.add(i) = c[i]; } }
    1
}

// ---------- gradients ----------

#[no_mangle]
pub extern "C" fn bvg_gradient_count(h: *const BvgHandle) -> usize {
    if h.is_null() { return 0; }
    unsafe { (*h).grads.len() }
}

#[no_mangle]
pub extern "C" fn bvg_gradient_get(h: *const BvgHandle, idx: usize,
                                   out_kind: *mut c_int, out_stop_count: *mut usize,
                                   out_stops_off: *mut f64, out_stops_ci: *mut u16,
                                   out_stops_rgba: *mut u8, out_geo: *mut f64) -> c_int {
    if h.is_null() { return 0; }
    let hh = unsafe { &*h };
    if idx >= hh.grads.len() { return 0; }
    let g = &hh.grads[idx];
    unsafe {
        if !out_kind.is_null() { *out_kind = g.kind as c_int; }
        if !out_stop_count.is_null() { *out_stop_count = g.stops.len(); }
    }
    for (i, st) in g.stops.iter().enumerate() {
        unsafe {
            if !out_stops_off.is_null() { *out_stops_off.add(i) = st.offset as f64; }
            if !out_stops_ci.is_null() { *out_stops_ci.add(i) = st.color_idx; }
            if !out_stops_rgba.is_null() {
                for k in 0..4 { *out_stops_rgba.add(i * 4 + k) = st.rgba[k]; }
            }
        }
    }
    if !out_geo.is_null() {
        for (i, v) in g.geometry.iter().enumerate() {
            if i >= 5 { break; }
            unsafe { *out_geo.add(i) = *v; }
        }
    }
    1
}

// ---------- styles ----------

#[no_mangle]
pub extern "C" fn bvg_style_count(h: *const BvgHandle) -> usize {
    if h.is_null() { return 0; }
    unsafe { (*h).styles.len() }
}

#[no_mangle]
pub extern "C" fn bvg_style_get(h: *const BvgHandle, idx: usize,
                                out_fill_kind: *mut c_int, out_fill_idx: *mut u16,
                                out_stroke_kind: *mut c_int, out_stroke_idx: *mut u16,
                                out_sw: *mut f64, out_cap: *mut c_int, out_join: *mut c_int,
                                out_miter: *mut f64, out_opacity: *mut u8, out_blend: *mut c_int,
                                out_dash_count: *mut usize, out_dash: *mut f64) -> c_int {
    if h.is_null() { return 0; }
    let hh = unsafe { &*h };
    if idx >= hh.styles.len() { return 0; }
    let s = &hh.styles[idx];
    unsafe {
        if !out_fill_kind.is_null() { *out_fill_kind = s.fill_kind as c_int; }
        if !out_fill_idx.is_null() { *out_fill_idx = s.fill_idx; }
        if !out_stroke_kind.is_null() { *out_stroke_kind = s.stroke_kind as c_int; }
        if !out_stroke_idx.is_null() { *out_stroke_idx = s.stroke_idx; }
        if !out_sw.is_null() { *out_sw = s.stroke_width as f64; }
        if !out_cap.is_null() { *out_cap = s.cap as c_int; }
        if !out_join.is_null() { *out_join = s.join as c_int; }
        if !out_miter.is_null() { *out_miter = s.miter_limit as f64; }
        if !out_opacity.is_null() { *out_opacity = s.opacity; }
        if !out_blend.is_null() { *out_blend = s.blend_mode as c_int; }
        if !out_dash_count.is_null() { *out_dash_count = s.dash.len(); }
        for (i, d) in s.dash.iter().enumerate() {
            if !out_dash.is_null() { *out_dash.add(i) = *d as f64; }
        }
    }
    1
}

// ---------- images ----------

#[no_mangle]
pub extern "C" fn bvg_images_count(h: *const BvgHandle) -> usize {
    if h.is_null() { return 0; }
    unsafe { (*h).images.len() }
}

#[no_mangle]
pub extern "C" fn bvg_images_get(h: *const BvgHandle, idx: usize,
                                 out_format: *mut c_int, out_w: *mut u16, out_h: *mut u16,
                                 out_data: *mut u8, buf_len: usize) -> c_int {
    if h.is_null() { return 0; }
    let hh = unsafe { &*h };
    if idx >= hh.images.len() { return 0; }
    let im = &hh.images[idx];
    unsafe {
        if !out_format.is_null() { *out_format = im.format as c_int; }
        if !out_w.is_null() { *out_w = im.width; }
        if !out_h.is_null() { *out_h = im.height; }
    }
    let n = im.data.len().min(buf_len);
    if !out_data.is_null() && n > 0 {
        unsafe { std::ptr::copy_nonoverlapping(im.data.as_ptr(), out_data, n); }
    }
    n as c_int
}