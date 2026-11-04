import init, { WasmBvg } from './pkg/bvg_wasm.js';

let bvg = null;
let zoom = 1.0;
let panX = 0, panY = 0;
let dragging = false;
let lastX = 0, lastY = 0;
let isRawMode = false;

const wrap = document.getElementById('canvas-wrap');
const canvas = document.getElementById('canvas');
const info = document.getElementById('info');
const statusFile = document.getElementById('status-file');
const statusSize = document.getElementById('status-size');
const statusZoom = document.getElementById('status-zoom');
const zoomLabel = document.getElementById('zoom-label');

function updateUI() {
    if (isRawMode || !zoomLabel || !statusZoom) return;
    zoomLabel.textContent = `${Math.round(zoom * 100)}%`;
    statusZoom.innerHTML = `<i class="bi bi-search"></i> Zoom: ${Math.round(zoom * 100)}%`;
}

function fit() {
    if (!bvg || isRawMode) return;
    const sx = wrap.clientWidth / bvg.width;
    const sy = wrap.clientHeight / bvg.height;
    zoom = Math.min(sx, sy) * 0.95;
    panX = (wrap.clientWidth - bvg.width * zoom) / 2;
    panY = (wrap.clientHeight - bvg.height * zoom) / 2;
    redraw();
}

function reset() {
    zoom = 1.0; panX = 0; panY = 0;
    redraw();
}

function redraw() {
    if (!bvg || isRawMode) return;
    canvas.style.transform = `translate(${panX}px, ${panY}px) scale(${zoom})`;
    updateUI();
}

async function main() {
    await init();

    // --- RAW MODE CHECK ---
    const urlParams = new URLSearchParams(window.location.search);
    isRawMode = urlParams.get('raw') === 'true';
    const fileUrl = urlParams.get('file') || urlParams.get('url');

    if (isRawMode) {
        // Скрываем UI
        document.querySelectorAll('.ui-element').forEach(el => {
            el.style.display = 'none';
        });

        // Настраиваем контейнер для левого верхнего угла
        document.body.style.overflow = 'hidden';
        document.body.style.margin = '0';
        document.body.style.padding = '0';

        canvas.style.boxShadow = 'none';
        canvas.style.border = 'none';
        canvas.style.transform = 'none';
        canvas.style.display = 'block';
        canvas.style.margin = '0';
    }

    // --- LOAD FILE FROM URL (raw mode) ---
    if (fileUrl) {
        try {
            const resp = await fetch(fileUrl);
            if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
            const buf = new Uint8Array(await resp.arrayBuffer());

            if (bvg) { bvg.free(); bvg = null; }
            bvg = new WasmBvg(buf);
            await bvg.load_images();

            canvas.width = bvg.width;
            canvas.height = bvg.height;
            bvg.render('canvas');

            if (!isRawMode) {
                if (info) info.textContent = `${fileUrl} — ${bvg.width}×${bvg.height}`;
                fit();
            }
        } catch (err) {
            console.error("Failed to load file:", err);
            if (!isRawMode && info) {
                info.textContent = 'Error: ' + err.message;
            }
        }
    }

    // --- STANDARD FILE INPUT (non-raw mode) ---
    if (!isRawMode) {
        const fileInput = document.getElementById('file');
        if (fileInput) {
            fileInput.addEventListener('change', async (e) => {
                const file = e.target.files[0];
                if (!file) return;
                try {
                    const buf = new Uint8Array(await file.arrayBuffer());
                    if (bvg) { bvg.free(); bvg = null; }
                    bvg = new WasmBvg(buf);
                    await bvg.load_images();
                    if (info) info.textContent = `${file.name} — ${bvg.width}×${bvg.height}, nodes=${bvg.node_count}`;
                    if (statusFile) statusFile.innerHTML = `<i class="bi bi-file-earmark"></i> ${file.name}`;
                    if (statusSize) statusSize.innerHTML = `<i class="bi bi-aspect-ratio"></i> ${bvg.width}×${bvg.height}`;
                    canvas.width = bvg.width;
                    canvas.height = bvg.height;
                    bvg.render('canvas');
                    fit();
                } catch (err) {
                    if (info) info.textContent = 'Error: ' + err;
                    console.error(err);
                }
            });
        }

        wrap.addEventListener('wheel', (e) => {
            if (!bvg) return;
            e.preventDefault();
            const factor = e.deltaY < 0 ? 1.1 : 0.9;
            zoom = Math.max(0.05, Math.min(50, zoom * factor));
            redraw();
        }, { passive: false });

        wrap.addEventListener('mousedown', (e) => { dragging = true; lastX = e.clientX; lastY = e.clientY; });
        window.addEventListener('mouseup', () => { dragging = false; });
        window.addEventListener('mousemove', (e) => {
            if (!dragging) return;
            panX += e.clientX - lastX;
            panY += e.clientY - lastY;
            lastX = e.clientX; lastY = e.clientY;
            redraw();
        });

        const btnZoomIn = document.getElementById('btn-zoom-in');
        const btnZoomOut = document.getElementById('btn-zoom-out');
        const btnFit = document.getElementById('btn-fit');
        const btnReset = document.getElementById('btn-reset');

        if (btnZoomIn) btnZoomIn.onclick = () => { zoom = Math.min(50, zoom * 1.25); redraw(); };
        if (btnZoomOut) btnZoomOut.onclick = () => { zoom = Math.max(0.05, zoom / 1.25); redraw(); };
        if (btnFit) btnFit.onclick = fit;
        if (btnReset) btnReset.onclick = reset;
    }

    updateUI();
}

main();