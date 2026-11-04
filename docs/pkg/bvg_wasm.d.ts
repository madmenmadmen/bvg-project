/* tslint:disable */
/* eslint-disable */

export class WasmBvg {
    free(): void;
    [Symbol.dispose](): void;
    load_images(): Promise<void>;
    constructor(data: Uint8Array | number[]);
    render(canvas_id: string): void;
    readonly chunks: string;
    readonly height: number;
    readonly node_count: number;
    readonly width: number;
}

export function bvg_dump(data: Uint8Array | number[]): string;

export function bvg_info(data: Uint8Array | number[]): string;

export type InitInput = RequestInfo | URL | Response | BufferSource | WebAssembly.Module;

export interface InitOutput {
    readonly memory: WebAssembly.Memory;
    readonly __wbg_wasmbvg_free: (a: number, b: number) => void;
    readonly bvg_dump: (a: number, b: number) => [number, number, number, number];
    readonly bvg_info: (a: number, b: number) => [number, number, number, number];
    readonly wasmbvg_chunks: (a: number) => [number, number];
    readonly wasmbvg_height: (a: number) => number;
    readonly wasmbvg_load_images: (a: number) => any;
    readonly wasmbvg_new: (a: number, b: number) => [number, number, number];
    readonly wasmbvg_node_count: (a: number) => number;
    readonly wasmbvg_render: (a: number, b: number, c: number) => [number, number];
    readonly wasmbvg_width: (a: number) => number;
    readonly rust_zstd_wasm_shim_calloc: (a: number, b: number) => number;
    readonly rust_zstd_wasm_shim_free: (a: number) => void;
    readonly rust_zstd_wasm_shim_malloc: (a: number) => number;
    readonly rust_zstd_wasm_shim_memcmp: (a: number, b: number, c: number) => number;
    readonly rust_zstd_wasm_shim_memcpy: (a: number, b: number, c: number) => number;
    readonly rust_zstd_wasm_shim_memmove: (a: number, b: number, c: number) => number;
    readonly rust_zstd_wasm_shim_memset: (a: number, b: number, c: number) => number;
    readonly rust_zstd_wasm_shim_qsort: (a: number, b: number, c: number, d: number) => void;
    readonly wasm_bindgen_c95f835529ef238d___convert__closures_____invoke___js_sys_4f52226bf96fefaa___Function_fn_wasm_bindgen_c95f835529ef238d___JsValue_____wasm_bindgen_c95f835529ef238d___sys__Undefined___js_sys_4f52226bf96fefaa___Function_fn_wasm_bindgen_c95f835529ef238d___JsValue_____wasm_bindgen_c95f835529ef238d___sys__Undefined_______true_: (a: number, b: number, c: any, d: any) => void;
    readonly wasm_bindgen_c95f835529ef238d___convert__closures_____invoke___wasm_bindgen_c95f835529ef238d___JsValue__core_a09a0304ffb65c03___result__Result_____wasm_bindgen_c95f835529ef238d___JsError___true_: (a: number, b: number, c: any) => [number, number];
    readonly __wbindgen_exn_store: (a: number) => void;
    readonly __externref_table_alloc: () => number;
    readonly __wbindgen_externrefs: WebAssembly.Table;
    readonly __wbindgen_free: (a: number, b: number, c: number) => void;
    readonly __wbindgen_malloc: (a: number, b: number) => number;
    readonly __wbindgen_realloc: (a: number, b: number, c: number, d: number) => number;
    readonly __wbindgen_destroy_closure: (a: number, b: number) => void;
    readonly __externref_table_dealloc: (a: number) => void;
    readonly __wbindgen_start: () => void;
}

export type SyncInitInput = BufferSource | WebAssembly.Module;

/**
 * Instantiates the given `module`, which can either be bytes or
 * a precompiled `WebAssembly.Module`.
 *
 * @param {{ module: SyncInitInput }} module - Passing `SyncInitInput` directly is deprecated.
 *
 * @returns {InitOutput}
 */
export function initSync(module: { module: SyncInitInput } | SyncInitInput): InitOutput;

/**
 * If `module_or_path` is {RequestInfo} or {URL}, makes a request and
 * for everything else, calls `WebAssembly.instantiate` directly.
 *
 * @param {{ module_or_path: InitInput | Promise<InitInput> }} module_or_path - Passing `InitInput` directly is deprecated.
 *
 * @returns {Promise<InitOutput>}
 */
export default function __wbg_init (module_or_path?: { module_or_path: InitInput | Promise<InitInput> } | InitInput | Promise<InitInput>): Promise<InitOutput>;
