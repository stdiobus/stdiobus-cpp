#!/usr/bin/env node
/**
 * Echo worker for stdio Bus conformance tests.
 * Reads NDJSON (newline-delimited JSON) from stdin.
 * For JSON-RPC requests (with "id"), responds with a result echoing params.
 * Preserves sessionId and pool fields for session affinity routing.
 * For notifications (no "id"), silently consumes them.
 */

const readline = require('readline');

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
    terminal: false
});

rl.on('line', (line) => {
    if (!line.trim()) return;

    try {
        const msg = JSON.parse(line);
        // If it's a JSON-RPC request (has id), echo it back as a response
        if (msg.id !== undefined) {
            const response = {
                jsonrpc: "2.0",
                id: msg.id,
                result: msg.params || {}
            };
            // Preserve sessionId for session affinity routing
            if (msg.sessionId !== undefined) {
                response.sessionId = msg.sessionId;
            }
            // Preserve pool field
            if (msg.pool !== undefined) {
                response.pool = msg.pool;
            }
            process.stdout.write(JSON.stringify(response) + '\n');
        }
        // Notifications (no id) are consumed silently — no response
    } catch (e) {
        // If not valid JSON, echo the line back as-is
        process.stdout.write(line + '\n');
    }
});

rl.on('close', () => {
    process.exit(0);
});
