const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const net = require('net');
const path = require('path');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ 
    server,
    perMessageDeflate: false // Disable compression for mobile compatibility
});

const GAME_SERVER_HOST = '127.0.0.1';
const GAME_SERVER_PORT = 12345;
const WEB_PORT = process.env.PORT || 3001;

// Track active clients (ws -> tcpClient mappings)
const clients = new Set();
const maxClients = 50; // Limit active connections

// Heartbeat interval to clean dead connections
const heartbeat = setInterval(() => {
    clients.forEach((client) => {
        if (client.ws.readyState === WebSocket.OPEN) {
            client.ws.ping(); // Send low-level PING
        } else if (client.ws.readyState === WebSocket.CLOSED || client.ws.readyState === WebSocket.CLOSING) {
             // Force cleanup if stuck in closing
             client.cleanup();
        }
    });
}, 10000);

// Serve static files with no cache
app.use((req, res, next) => {
    res.set('Cache-Control', 'no-store, no-cache, must-revalidate, private');
    next();
});
app.use(express.static(path.join(__dirname, 'public')));

app.get('/health', (req, res) => {
    res.send(`OK ${Date.now()} from ${req.ip}`);
});

// logging upgrades
server.on('upgrade', (request, socket, head) => {
    console.log(`HTTP Upgrade request from ${request.socket.remoteAddress}`);
});

wss.on('connection', (ws, req) => {
    const ip = req.socket.remoteAddress;
    console.log(`Web client connected from ${ip} (Total: ${clients.size + 1})`);
    
    // Connect to C Game Server
    const tcpClient = new net.Socket();
    // tcpClient.setNoDelay(true); // Optional: try disabling if issues persist
    
    // Track this connection
    const clientData = { ws, tcpClient, cleanup: null };
    
    const cleanup = () => {
        if (clients.has(clientData)) {
            clients.delete(clientData);
            try { ws.close(); } catch(e){}
            try { tcpClient.destroy(); } catch(e){}
        }
    };
    clientData.cleanup = cleanup;
    clients.add(clientData);

    ws.on('close', (code, reason) => {
        console.log(`Web client disconnected (Code: ${code}, Reason: ${reason})`);
        cleanup();
    });

    ws.on('error', (e) => {
        console.error("WS Error:", e);
        cleanup();
    })

    // Immediate Hello
    if (ws.readyState === WebSocket.OPEN) {
        ws.send("PROXY_HELLO\n");
    }
    
    tcpClient.connect(GAME_SERVER_PORT, GAME_SERVER_HOST, () => {
        console.log('Connected to Game Server via TCP');
    });

    // TCP -> Web

    // TCP -> Web
    tcpClient.on('data', (data) => {
        // ... (data handling code) ...
        // We buffer or direct send. 
        // For game protocol line-based is best handled by client JS generally if simple
        // but here we just pass raw chunks.
        try {
            if (ws.readyState === WebSocket.OPEN) {
                ws.send(data.toString());
            }
        } catch (e) {
            console.error("WS Send Error:", e);
        }
    });

    tcpClient.on('close', () => {
        console.log('Game Server closed connection');
        cleanup();
    });

    tcpClient.on('error', (err) => {
        console.error('Game Server connection error:', err.message);
        cleanup();
    });

    // Web -> TCP
    ws.on('message', (message) => {
        const msgStr = message.toString();
        try {
            // Ensure newline
             if (!tcpClient.destroyed && tcpClient.writable) {
                if (!msgStr.endsWith('\n')) {
                    tcpClient.write(msgStr + '\n');
                } else {
                    tcpClient.write(msgStr);
                }
             }
        } catch (e) {
             console.error("TCP Write Error:", e);
        }
    });

    ws.on('close', () => {
        console.log('Web client disconnected');
        cleanup();
    });
    
    ws.on('error', (err) => {
        console.error('Web Socket error:', err);
        cleanup();
    });
});

// Handle graceful shutdown for SIGINT (Ctrl+C) and SIGTERM
if (process.platform === "win32") {
    const rl = require("readline").createInterface({
        input: process.stdin,
        output: process.stdout
    });

    rl.on("SIGINT", () => {
        process.emit("SIGINT");
    });
}

function shutdown() {
    console.log('\nShutting down web server...');
    const quitMsg = "QUIT\n";
    
    for (const client of clients) {
        try {
            // Force close TCP connection
            client.tcpClient.end(); 
            client.ws.close();
        } catch (e) {
            console.error("Error closing client:", e);
        }
    }
    
    server.close(() => {
        console.log('Server closed.');
        process.exit(0);
    });
    
    // Force exit if it takes too long
    setTimeout(() => {
        console.error("Forcing shutdown...");
        process.exit(1);
    }, 2000);
}

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);

server.listen(WEB_PORT, '0.0.0.0', () => {
    console.log(`Web server running on http://0.0.0.0:${WEB_PORT}`);
});
