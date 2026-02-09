// Bcash Block Explorer
// Single-file Node.js web server - no npm dependencies
// Queries the bcash JSON-RPC at localhost:9332

const http = require('http');

const RPC_HOST = '127.0.0.1';
const RPC_PORT = 9332;
const LISTEN_PORT = 3000;
const BLOCKS_PER_PAGE = 20;

// --- JSON-RPC helper ---

function rpcCall(method, params) {
    return new Promise((resolve, reject) => {
        const body = JSON.stringify({ method, params, id: 1 });
        const req = http.request({
            hostname: RPC_HOST,
            port: RPC_PORT,
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Content-Length': Buffer.byteLength(body),
            },
        }, (res) => {
            let data = '';
            res.on('data', (chunk) => data += chunk);
            res.on('end', () => {
                try {
                    const json = JSON.parse(data);
                    if (json.error) reject(new Error(json.error.message));
                    else resolve(json.result);
                } catch (e) {
                    reject(new Error('RPC parse error: ' + data.slice(0, 200)));
                }
            });
        });
        req.on('error', (e) => reject(e));
        req.write(body);
        req.end();
    });
}

// --- HTML rendering ---

const CSS = `
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    background: #0a0a0a; color: #c0c0c0; font-family: 'Courier New', monospace;
    font-size: 14px; line-height: 1.6;
}
a { color: #5fafff; text-decoration: none; }
a:hover { text-decoration: underline; color: #87cfff; }
.wrap { max-width: 960px; margin: 0 auto; padding: 0 16px; }
header {
    background: #111; border-bottom: 1px solid #333; padding: 12px 0;
    margin-bottom: 24px;
}
header .wrap { display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 8px; }
header h1 { color: #e0e0e0; font-size: 18px; font-weight: bold; }
header h1 span { color: #5fafff; }
nav a { margin-left: 16px; color: #888; font-size: 13px; }
nav a:hover { color: #5fafff; }
.search-form { display: flex; gap: 4px; }
.search-form input {
    background: #1a1a1a; border: 1px solid #333; color: #c0c0c0;
    font-family: inherit; font-size: 13px; padding: 4px 8px; width: 260px;
}
.search-form button {
    background: #222; border: 1px solid #333; color: #888;
    font-family: inherit; font-size: 13px; padding: 4px 10px; cursor: pointer;
}
.search-form button:hover { color: #5fafff; border-color: #5fafff; }
h2 { color: #e0e0e0; font-size: 15px; margin-bottom: 12px; border-bottom: 1px solid #222; padding-bottom: 6px; }
.card {
    background: #111; border: 1px solid #222; padding: 14px; margin-bottom: 16px;
    border-radius: 2px;
}
.card h3 { color: #e0e0e0; font-size: 14px; margin-bottom: 8px; }
table { width: 100%; border-collapse: collapse; }
th { text-align: left; color: #888; font-weight: normal; padding: 4px 8px; border-bottom: 1px solid #222; font-size: 12px; }
td { padding: 4px 8px; border-bottom: 1px solid #1a1a1a; font-size: 13px; word-break: break-all; }
.mono { font-family: 'Courier New', monospace; }
.label { color: #888; display: inline-block; min-width: 140px; }
.value { color: #e0e0e0; }
.row { margin-bottom: 4px; }
.pager { margin: 16px 0; text-align: center; }
.pager a {
    display: inline-block; padding: 4px 12px; margin: 0 4px;
    background: #1a1a1a; border: 1px solid #333; color: #888; font-size: 13px;
}
.pager a:hover { color: #5fafff; border-color: #5fafff; text-decoration: none; }
.coinbase-tag { color: #d4a017; font-size: 11px; }
.stat { display: inline-block; margin-right: 24px; margin-bottom: 8px; }
.stat .num { color: #5fafff; font-size: 16px; }
.stat .lbl { color: #666; font-size: 11px; }
footer { border-top: 1px solid #222; padding: 12px 0; margin-top: 24px; color: #444; font-size: 11px; text-align: center; }
.err { color: #ff5f5f; }
`;

function renderPage(title, bodyHtml) {
    return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(title)} - Bcash Explorer</title>
<style>${CSS}</style>
</head>
<body>
<header>
<div class="wrap">
<h1><span>B</span>cash Explorer</h1>
<div style="display:flex;align-items:center;gap:12px;flex-wrap:wrap;">
<nav>
<a href="/">Dashboard</a>
<a href="/blocks">Blocks</a>
</nav>
<form class="search-form" action="/search" method="get">
<input type="text" name="q" placeholder="Search block hash, tx, height...">
<button type="submit">Go</button>
</form>
</div>
</div>
</header>
<div class="wrap">
${bodyHtml}
</div>
<footer><div class="wrap">bcash block explorer</div></footer>
</body>
</html>`;
}

function esc(s) {
    if (s == null) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function formatTime(ts) {
    if (!ts) return 'N/A';
    return new Date(ts * 1000).toISOString().replace('T', ' ').replace(/\.\d+Z$/, ' UTC');
}

// --- Route handlers ---

async function handleDashboard() {
    const info = await rpcCall('getblockchaininfo', []);
    const height = info.blocks;

    // Fetch last 10 blocks
    let blocksHtml = '';
    const start = height;
    const end = Math.max(0, height - 9);
    for (let h = start; h >= end; h--) {
        const hash = await rpcCall('getblockhash', [h]);
        const block = await rpcCall('getblock', [hash]);
        blocksHtml += `<tr>
            <td>${block.height}</td>
            <td><a href="/block/${esc(block.hash)}">${esc(block.hash.slice(0, 16))}...</a></td>
            <td>${block.txcount}</td>
            <td>${formatTime(block.time)}</td>
        </tr>`;
    }

    const body = `
    <h2>Chain Overview</h2>
    <div class="card">
        <div class="stat"><div class="num">${esc(String(info.blocks))}</div><div class="lbl">HEIGHT</div></div>
        <div class="stat"><div class="num">${esc(String(info.difficulty))}</div><div class="lbl">DIFFICULTY</div></div>
        <div class="stat"><div class="num">${esc(String(info.mempoolsize))}</div><div class="lbl">MEMPOOL</div></div>
        <div class="row" style="margin-top:8px;">
            <span class="label">Best Block</span>
            <a href="/block/${esc(info.bestblockhash)}">${esc(info.bestblockhash)}</a>
        </div>
    </div>
    <h2>Recent Blocks</h2>
    <div class="card">
    <table>
        <tr><th>Height</th><th>Hash</th><th>Txs</th><th>Time</th></tr>
        ${blocksHtml}
    </table>
    </div>`;
    return renderPage('Dashboard', body);
}

async function handleBlocks(query) {
    const info = await rpcCall('getblockchaininfo', []);
    const height = info.blocks;

    const page = Math.max(1, parseInt(query.page) || 1);
    const startHeight = height - (page - 1) * BLOCKS_PER_PAGE;
    const endHeight = Math.max(0, startHeight - BLOCKS_PER_PAGE + 1);

    let rows = '';
    for (let h = startHeight; h >= endHeight; h--) {
        const hash = await rpcCall('getblockhash', [h]);
        const block = await rpcCall('getblock', [hash]);
        rows += `<tr>
            <td>${block.height}</td>
            <td><a href="/block/${esc(block.hash)}">${esc(block.hash.slice(0, 20))}...</a></td>
            <td>${block.txcount}</td>
            <td>${formatTime(block.time)}</td>
        </tr>`;
    }

    let pager = '<div class="pager">';
    if (page > 1) pager += `<a href="/blocks?page=${page - 1}">&laquo; Newer</a>`;
    pager += `<span style="color:#666;margin:0 8px;">Page ${page}</span>`;
    if (endHeight > 0) pager += `<a href="/blocks?page=${page + 1}">Older &raquo;</a>`;
    pager += '</div>';

    const body = `
    <h2>All Blocks</h2>
    ${pager}
    <div class="card">
    <table>
        <tr><th>Height</th><th>Hash</th><th>Txs</th><th>Time</th></tr>
        ${rows}
    </table>
    </div>
    ${pager}`;
    return renderPage('Blocks', body);
}

async function handleBlock(hash) {
    const block = await rpcCall('getblock', [hash]);

    let txRows = '';
    for (let i = 0; i < block.tx.length; i++) {
        const tx = block.tx[i];
        txRows += `<tr>
            <td>${i}</td>
            <td><a href="/tx/${esc(tx.txid)}">${esc(tx.txid.slice(0, 20))}...</a></td>
            <td>${tx.coinbase ? '<span class="coinbase-tag">coinbase</span>' : ''}${tx.coinbase && tx.value ? ' ' + esc(tx.value) + ' BCS' : ''}</td>
        </tr>`;
    }

    let navLinks = '';
    if (block.previousblockhash && block.previousblockhash !== '0000000000000000000000000000000000000000000000000000000000000000')
        navLinks += `<a href="/block/${esc(block.previousblockhash)}">&laquo; Prev</a> `;
    if (block.nextblockhash)
        navLinks += `<a href="/block/${esc(block.nextblockhash)}">Next &raquo;</a>`;

    const body = `
    <h2>Block #${block.height} ${navLinks}</h2>
    <div class="card">
        <div class="row"><span class="label">Hash</span> <span class="value">${esc(block.hash)}</span></div>
        <div class="row"><span class="label">Height</span> <span class="value">${block.height}</span></div>
        <div class="row"><span class="label">Version</span> <span class="value">${block.version}</span></div>
        <div class="row"><span class="label">Timestamp</span> <span class="value">${formatTime(block.time)}</span></div>
        <div class="row"><span class="label">Prev Block</span> <a href="/block/${esc(block.previousblockhash)}">${esc(block.previousblockhash)}</a></div>
        <div class="row"><span class="label">Merkle Root</span> <span class="value">${esc(block.merkleroot)}</span></div>
        <div class="row"><span class="label">Bits</span> <span class="value">${block.bits}</span></div>
        <div class="row"><span class="label">Nonce</span> <span class="value">${block.nonce}</span></div>
        <div class="row"><span class="label">Transactions</span> <span class="value">${block.txcount}</span></div>
    </div>
    <h2>Transactions</h2>
    <div class="card">
    <table>
        <tr><th>#</th><th>Txid</th><th>Info</th></tr>
        ${txRows}
    </table>
    </div>`;
    return renderPage('Block ' + block.height, body);
}

async function handleTx(txid) {
    const tx = await rpcCall('getrawtransaction', [txid]);

    let vinRows = '';
    for (let i = 0; i < tx.vin.length; i++) {
        const inp = tx.vin[i];
        if (inp.coinbase) {
            vinRows += `<tr><td>${i}</td><td colspan="2"><span class="coinbase-tag">coinbase</span></td></tr>`;
        } else {
            vinRows += `<tr>
                <td>${i}</td>
                <td><a href="/tx/${esc(inp.txid)}">${esc(inp.txid.slice(0, 20))}...</a></td>
                <td>${inp.vout}</td>
            </tr>`;
        }
    }

    let voutRows = '';
    for (let i = 0; i < tx.vout.length; i++) {
        const out = tx.vout[i];
        const addr = out.address ? `<a href="/search?q=${esc(out.address)}">${esc(out.address)}</a>` : '<span style="color:#666">N/A</span>';
        voutRows += `<tr>
            <td>${out.n}</td>
            <td>${esc(out.value)} BCS</td>
            <td>${addr}</td>
        </tr>`;
    }

    const body = `
    <h2>Transaction</h2>
    <div class="card">
        <div class="row"><span class="label">Txid</span> <span class="value">${esc(tx.txid)}</span></div>
        <div class="row"><span class="label">Version</span> <span class="value">${tx.version}</span></div>
        <div class="row"><span class="label">Lock Time</span> <span class="value">${tx.locktime}</span></div>
        <div class="row"><span class="label">Coinbase</span> <span class="value">${tx.coinbase ? 'Yes' : 'No'}</span></div>
        <div class="row"><span class="label">Block</span> <a href="/block/${esc(tx.blockhash)}">${esc(tx.blockhash)}</a></div>
        <div class="row"><span class="label">Block Height</span> <span class="value">${tx.blockheight}</span></div>
    </div>
    <h2>Inputs (${tx.vin.length})</h2>
    <div class="card">
    <table>
        <tr><th>#</th><th>Prev Tx</th><th>Vout</th></tr>
        ${vinRows}
    </table>
    </div>
    <h2>Outputs (${tx.vout.length})</h2>
    <div class="card">
    <table>
        <tr><th>#</th><th>Value</th><th>Address</th></tr>
        ${voutRows}
    </table>
    </div>`;
    return renderPage('Tx ' + txid.slice(0, 12) + '...', body);
}

async function handleSearch(q) {
    q = (q || '').trim();
    if (!q) return renderPage('Search', '<div class="card"><p>Enter a block hash, transaction hash, block height, or address.</p></div>');

    // Try as block height (pure number)
    if (/^\d+$/.test(q)) {
        try {
            const hash = await rpcCall('getblockhash', [parseInt(q)]);
            return null; // redirect
        } catch (e) { /* not a valid height */ }
    }

    // Try as block hash (64 hex chars)
    if (/^[0-9a-fA-F]{64}$/.test(q)) {
        try {
            await rpcCall('getblock', [q]);
            return null; // redirect to block
        } catch (e) { /* not a block hash */ }

        try {
            await rpcCall('getrawtransaction', [q]);
            return null; // redirect to tx
        } catch (e) { /* not a tx hash either */ }
    }

    return renderPage('Search', `<div class="card"><p class="err">No results found for "${esc(q)}"</p></div>`);
}

// --- HTTP server ---

const server = http.createServer(async (req, res) => {
    try {
        const url = new URL(req.url, 'http://localhost');
        const path = url.pathname;
        const query = Object.fromEntries(url.searchParams);

        let html;

        if (path === '/' || path === '') {
            html = await handleDashboard();
        } else if (path === '/blocks') {
            html = await handleBlocks(query);
        } else if (path.startsWith('/block/')) {
            const hash = path.slice(7);
            html = await handleBlock(hash);
        } else if (path.startsWith('/tx/')) {
            const txid = path.slice(4);
            html = await handleTx(txid);
        } else if (path === '/search') {
            const q = query.q || '';

            // Handle search with redirects
            if (/^\d+$/.test(q.trim())) {
                try {
                    const hash = await rpcCall('getblockhash', [parseInt(q.trim())]);
                    res.writeHead(302, { Location: '/block/' + hash });
                    res.end();
                    return;
                } catch (e) { /* fall through */ }
            }

            if (/^[0-9a-fA-F]{64}$/.test(q.trim())) {
                try {
                    await rpcCall('getblock', [q.trim()]);
                    res.writeHead(302, { Location: '/block/' + q.trim() });
                    res.end();
                    return;
                } catch (e) { /* not a block */ }

                try {
                    await rpcCall('getrawtransaction', [q.trim()]);
                    res.writeHead(302, { Location: '/tx/' + q.trim() });
                    res.end();
                    return;
                } catch (e) { /* not a tx */ }
            }

            html = renderPage('Search', `<div class="card"><p class="err">No results found for "${esc(q)}"</p></div>`);
        } else {
            res.writeHead(404, { 'Content-Type': 'text/html' });
            res.end(renderPage('Not Found', '<div class="card"><p class="err">404 - Page not found</p></div>'));
            return;
        }

        res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
        res.end(html);
    } catch (err) {
        console.error('Error:', err.message);
        res.writeHead(500, { 'Content-Type': 'text/html' });
        res.end(renderPage('Error', `<div class="card"><p class="err">Error: ${esc(err.message)}</p></div>`));
    }
});

server.listen(LISTEN_PORT, '127.0.0.1', () => {
    console.log(`Bcash Block Explorer running at http://127.0.0.1:${LISTEN_PORT}`);
});
