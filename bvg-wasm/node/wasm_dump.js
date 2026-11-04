import fs from 'fs';
import { bvg_dump } from './pkg/bvg_wasm.js';

async function main() {
    let args = process.argv.slice(2);
    if (args[0] === 'dump') args = args.slice(1);
    const path = args[0];
    if (!path) {
        console.error('usage: node wasm_dump.js dump <file>');
        process.exit(2);
    }
    const data = new Uint8Array(fs.readFileSync(path));
    const json = bvg_dump(data);
    process.stdout.write(json);
    if (!json.endsWith('\n')) process.stdout.write('\n');
}

main().catch(e => {
    console.error(e);
    process.exit(1);
});