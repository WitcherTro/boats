# Boats Game - Web Client

This is a simple Web Interface for the Boats Game Server.
It uses a Node.js proxy to bridge WebSocket connections (Browser) to TCP sockets (Game Server).

## Prerequisites
- **Node.js** (v14 or higher)
- The C Game Server running on port 12345 (default).

## Setup & Run

1. Open a terminal in this directory (`web/`).
2. Install dependencies:
   ```bash
   npm install
   ```
3. Start the Web Server:
   ```bash
   npm start
   ```
4. Open your browser to `http://localhost:3001`.

## Architecture
- `server.js`: Express server + WebSocket Server. Forwards all traffic blindly between WS clients and the TCP Game Server.
- `public/game.js`: Implements the Boats Text Protocol (BTP) over WebSocket.
