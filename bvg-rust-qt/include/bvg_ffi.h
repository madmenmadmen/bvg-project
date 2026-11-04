#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BvgHandle BvgHandle;

BvgHandle* bvg_open(const char* path);
void bvg_free(BvgHandle* h);

int bvg_width(const BvgHandle* h);
int bvg_height(const BvgHandle* h);

size_t bvg_node_count(const BvgHandle* h);
int bvg_node_type(const BvgHandle* h, size_t idx);
uint16_t bvg_node_style(const BvgHandle* h, size_t idx);
uint16_t bvg_node_depth(const BvgHandle* h, size_t idx);
uint16_t bvg_node_child_count(const BvgHandle* h, size_t idx);
int bvg_node_has_transform(const BvgHandle* h, size_t idx);
int bvg_node_transform(const BvgHandle* h, size_t idx, double* out6);

// Rect / Image: xywh
int bvg_node_rect(const BvgHandle* h, size_t idx, double* out4);
// Ellipse: cx, cy, rx, ry
int bvg_node_ellipse(const BvgHandle* h, size_t idx, double* out4);
// Line: x1, y1, x2, y2
int bvg_node_line(const BvgHandle* h, size_t idx, double* out4);
// Polyline/Polygon: n points
size_t bvg_node_point_count(const BvgHandle* h, size_t idx);
int bvg_node_points(const BvgHandle* h, size_t idx, double* out2n);
// Path: n commands; each cmd is (op, args[8])
size_t bvg_node_path_count(const BvgHandle* h, size_t idx);
int bvg_node_path(const BvgHandle* h, size_t idx,
                  uint8_t* out_ops, double* out_args);
// Text: x, y, utf8
int bvg_node_text(const BvgHandle* h, size_t idx,
                  double* out_xy, uint8_t* out_buf, size_t buf_len);
// Image: image_idx
int bvg_node_image(const BvgHandle* h, size_t idx, uint16_t* out_img);
// Use: def_idx, x, y
int bvg_node_use(const BvgHandle* h, size_t idx,
                 uint16_t* out_def, double* out_xy);

// Palette
size_t bvg_palette_size(const BvgHandle* h);
int bvg_palette_get(const BvgHandle* h, size_t idx, uint8_t* out_rgba);

// Gradients
size_t bvg_gradient_count(const BvgHandle* h);
int bvg_gradient_get(const BvgHandle* h, size_t idx,
                     int* out_kind, size_t* out_stop_count,
                     double* out_stops_off, uint16_t* out_stops_ci,
                     uint8_t* out_stops_rgba, double* out_geo);

// Styles
size_t bvg_style_count(const BvgHandle* h);
int bvg_style_get(const BvgHandle* h, size_t idx,
                  int* out_fill_kind, uint16_t* out_fill_idx,
                  int* out_stroke_kind, uint16_t* out_stroke_idx,
                  double* out_sw, int* out_cap, int* out_join,
                  double* out_miter, uint8_t* out_opacity, int* out_blend,
                  size_t* out_dash_count, double* out_dash);

// Images (IMGS)
size_t bvg_images_count(const BvgHandle* h);
int bvg_images_get(const BvgHandle* h, size_t idx,
                   int* out_format, uint16_t* out_w, uint16_t* out_h,
                   uint8_t* out_data, size_t buf_len);

#ifdef __cplusplus
}
#endif