const statusDiv = document.getElementById('status');
const messagesDiv = document.getElementById('messages');
const myGrid = document.getElementById('my-grid');
const opGrid = document.getElementById('op-grid');

const protocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
const wsUrl = protocol + window.location.host;
let ws;
let connected = false;

function connectWebSocket() {
    statusDiv.innerText = "Connecting...";
    statusDiv.style.color = 'orange';

    try {
        ws = new WebSocket(wsUrl);
    } catch (e) {
        console.error(e);
        setTimeout(connectWebSocket, 3000);
        return;
    }

    ws.onopen = () => {
        statusDiv.innerText = "Connected to Server...";
        statusDiv.style.color = 'orange';
        ws.send("HELLO");
    };

    ws.onclose = () => {
        statusDiv.innerText = "Disconnected. Retrying...";
        statusDiv.style.color = 'red';
        setTimeout(connectWebSocket, 2000);
    };

    ws.onerror = (err) => {
        console.error("WS Error", err);
    };

    ws.onmessage = (event) => {
        const raw = event.data;
        const lines = raw.split('\n');

        lines.forEach(line => {
            line = line.trim();
            if (!line) return;

            if (line === "PROXY_HELLO") {
                connected = true;
                statusDiv.innerHTML = "Connected! Select a Lobby...";
                statusDiv.style.color = '#4CAF50';

                // Send default name to satisfy server
                ws.send("NAME Player");

                // Auto-list lobbies and show lobby screen directly
                ws.send("LOBBY_LIST");
                // Auto-refresh lobbies
                setInterval(() => {
                    if (document.getElementById('lobby-screen').style.display === 'block') {
                        ws.send("LOBBY_LIST");
                    }
                }, 1000);

                document.getElementById('setup-screen').style.display = 'none';
                document.getElementById('lobby-screen').style.display = 'block';
                return;
            }

            handleServerMessage(line);
        });
    };
}

// Start connection
connectWebSocket();

let myPlayerId = -1;
let shipsToPlace = []; // Queue of lengths
let currentShipLen = 0;
let currentDirection = 'H';
let gameState = 'CONNECTING'; // CONNECTING, NAMING, WAITING_READY, PLACING, WAITING_TURN, FIRING, GAMEOVER
let isAutoPlacing = false;
let placedShips = []; // Array of {r, c, len, dir}
window.isLocalPlacementMode = true;

let tempLobbyList = [];

// Helper for chat
function logToChat(msg, color = '#eee') {
    const p = document.createElement('div');
    p.innerText = msg;
    p.style.color = color;
    messagesDiv.appendChild(p);

    // Force scroll to bottom (setTimeout ensures render is complete)
    setTimeout(() => {
        messagesDiv.scrollTop = messagesDiv.scrollHeight;
    }, 50);
}

// Mobile Tab Switching
function switchTab(targetId) {
    // Only matters on mobile
    if (window.innerWidth > 768) return;

    // Update Buttons
    const buttons = document.querySelectorAll('.tab-btn');
    buttons.forEach(b => {
        if (b.innerText.toLowerCase().includes(targetId.includes('my') ? 'fleet' : 'targeting')) {
            b.classList.add('active');
        } else {
            b.classList.remove('active');
        }
    });

    // Update Views
    document.getElementById('container-my-grid').classList.remove('active-view');
    document.getElementById('container-op-grid').classList.remove('active-view');

    document.getElementById('container-' + targetId).classList.add('active-view');
}

function resetGameUI() {
    // 1. Reset flagov (TOTO JE NAJDÔLEŽITEJŠIE)
    window.isLocalPlacementMode = true;
    window.isBatchSending = false; // <--- Ak toto ostalo true, nextShip() sa nikdy nespustí

    // 2. Vyčistenie mriežok (vizuálne aj dáta)
    document.querySelectorAll('.cell').forEach(c => {
        c.className = 'cell';
        delete c.dataset.shipR;
        delete c.dataset.shipC;
        delete c.dataset.shipLen;
        delete c.dataset.shipDir;
    });

    // 3. Reset herných premenných
    gameState = 'PLACING';
    placedShips = [];
    shipsToPlace = [];
    currentShipLen = 0;

    // 4. Reset UI prvkov
    // Skryjeme všetko, kým server nepošle START_PLACEMENT
    document.getElementById('place-controls').style.display = 'none';
    document.getElementById('fire-controls').style.display = 'none';

    statusDiv.innerText = "Game Restarted! Waiting for server...";

    // 5. Odstránenie tlačidiel z konca hry ("Play Again")
    const pab = document.getElementById('play-again-box');
    if (pab) pab.remove();

    // 6. Odstránenie tlačidla "Ready" ak tam náhodou ostalo
    const rb = document.getElementById('ready-btn'); // Predpokladám, že máš ID ready-btn
    if (rb) rb.remove();

    // Ak máš nejaký kontajner pre ready button, vyčisti ho
    // (Skontroluj, či createReadyButton nevytvára niečo, čo tu treba zmazať)
}

function nextShip() {
    if (window.isBatchSending) return;
    // If using Local Placement mode, we consume the queue locally
    if (shipsToPlace.length > 0) {
        currentShipLen = shipsToPlace.shift();
        gameState = 'PLACING';

        // Auto-switch to My Grid on Mobile
        switchTab('my-grid');

        document.getElementById('place-controls').style.display = 'block';
        document.getElementById('fire-controls').style.display = 'none';
        document.getElementById('current-ship-len').innerText = currentShipLen;
        statusDiv.innerText = `Place Ship of Length ${currentShipLen}`;

        if (isAutoPlacing) {
            // Local auto-place happens instantly in loop
            // setTimeout(tryRandomPlace, 100);
        }
    } else {
        // All ships placed LOCALLY
        gameState = 'WAITING_OFFSET'; // waiting for user to confirm
        isAutoPlacing = false;
        window.isLocalPlacementMode = true;
        currentShipLen = 0;
        statusDiv.innerText = "All ships placed. Press Ready or Adjust Ships.";

        // Show "I'm Ready" button which triggers the batch send
        createReadyButton(true);
    }
}

// ... existing code ...

function drawShip(gridElement, r, c, len, isVertical) {
    // ODSTRÁNENÉ: Žiadne if (gridElement === myGrid) { placedShips.push... }
    // Funkcia iba kreslí. O dáta sa stará server alebo eventy.

    for (let i = 0; i < len; i++) {
        let x = c;
        let y = r;
        if (isVertical) y += i;
        else x += i;

        // Kontrola hraníc mriežky
        const index = y * 9 + x;
        if (index >= 0 && index < gridElement.children.length) {
            const cell = gridElement.children[index];
            cell.classList.add('ship');

            // Nastavenie dát pre drag & drop
            cell.dataset.shipR = r;
            cell.dataset.shipC = c;
            cell.dataset.shipLen = len;
            cell.dataset.shipDir = isVertical ? 'V' : 'H';
        }
    }
}

function drawAllMyShips() {
    // 1. Najprv vyčistíme celú mriežku od starých lodí
    Array.from(myGrid.children).forEach(cell => {
        cell.classList.remove('ship');
        // Vymažeme aj dáta, aby sme nemohli chytať prázdne políčka
        delete cell.dataset.shipR;
        delete cell.dataset.shipC;
        delete cell.dataset.shipLen;
        delete cell.dataset.shipDir;
    });

    // 2. Prejdeme pole placedShips a nakreslíme ich nanovo
    placedShips.forEach(s => {
        // Pozor: v poli máš dir ako 'V' alebo 'H', drawShip očakáva boolean pre isVertical
        drawShip(myGrid, s.r, s.c, s.len, s.dir === 'V');
    });
}
// --- DRAG AND DROP LOGIC ---
let dragShip = null; // {r, c, len, dir, offsetX, offsetY}
let ghostElement = null;

function initGhost() {
    ghostElement = document.createElement('div');
    ghostElement.id = 'ghost-ship';
    document.body.appendChild(ghostElement);
}
initGhost();

function onDown(e) {
    if (gameState !== 'WAITING_READY' && gameState !== 'PLACING' && gameState !== 'WAITING_OFFSET') return;
    // If placement is not finished, maybe don't allow moving yet?
    // Server allows moving "before both players ready".
    // But usually only after placement is done do users refine.
    // However, let's allow it if we click a ship.

    const cell = e.target.closest('.cell');
    if (!cell || !cell.classList.contains('ship')) return;

    // Only my grid
    if (!myGrid.contains(cell)) return;

    // e.preventDefault(); // prevent scrolling/selecting - COMMENTED OUT FOR MOBILE SCROLE
    // Only prevent default if we DEFINITELY started a drag

    const r = parseInt(cell.dataset.shipR);
    const c = parseInt(cell.dataset.shipC);
    const len = parseInt(cell.dataset.shipLen);
    const dir = cell.dataset.shipDir;

    // Calculate offset index within ship
    const cellX = parseInt(cell.dataset.x);
    const cellY = parseInt(cell.dataset.y);

    let offset = 0;
    if (dir === 'H') offset = cellX - c;
    else offset = cellY - r;

    // Check if touch
    const clientX = e.clientX || (e.touches && e.touches[0].clientX);
    const clientY = e.clientY || (e.touches && e.touches[0].clientY);

    if (clientX === undefined) return; // Should not happen

    e.preventDefault(); // NOW prevent default

    dragShip = { r, c, len, dir, offset, startX: clientX, startY: clientY, startTime: Date.now() };

    // Visuals
    // Mark source cells as dragging source
    markShipCells(r, c, len, dir, 'dragging-source');

    // Setup ghost
    updateGhost(e);
}

function onMove(e) {
    if (!dragShip) return;
    e.preventDefault(); // Prevent scroll while dragging
    updateGhost(e);
}

let lastShipTapTime = 0;

function onUp(e) {
    if (!dragShip) return;

    // Logic to find drop target
    // For touches, use changedTouches
    const clientX = e.clientX || (e.changedTouches && e.changedTouches[0].clientX);
    const clientY = e.clientY || (e.changedTouches && e.changedTouches[0].clientY);

    // TAP DETECTION LOGIC
    // Check if moved significantly
    const dist = Math.sqrt(Math.pow(clientX - dragShip.startX, 2) + Math.pow(clientY - dragShip.startY, 2));
    const dur = Date.now() - dragShip.startTime;

    // Clear source visuals
    markShipCells(dragShip.r, dragShip.c, dragShip.len, dragShip.dir, 'dragging-source', true);
    ghostElement.style.display = 'none';

    if (dist < 10 && dur < 300) {
        // It was a TAP (start/end close in time and space)
        const now = Date.now();
        if (now - lastShipTapTime < 500) {
            // DOUBLE TAP -> Rotate
            onDblClick(e); // Reuse existing logic
            lastShipTapTime = 0; // Reset
        } else {
            lastShipTapTime = now;
        }

        // Stop here (don't process as a move/drop)
        dragShip = null;
        return;
    }

    const target = document.elementFromPoint(clientX, clientY);
    const cell = target ? target.closest('.cell') : null;

    if (cell && myGrid.contains(cell)) {
        const dropX = parseInt(cell.dataset.x);
        const dropY = parseInt(cell.dataset.y);

        // Calculate new head position based on offset
        let newR = dropY;
        let newC = dropX;

        if (dragShip.dir === 'H') newC -= dragShip.offset;
        else newR -= dragShip.offset;

        // Check bounds locally (optional, server validates too)

        if (window.isLocalPlacementMode) {
             handleLocalMove(dragShip.r, dragShip.c, newR, newC, dragShip.dir, dragShip.len);
        } else {
             ws.send(`MOVE ${dragShip.r} ${dragShip.c} ${newR} ${newC} ${dragShip.dir}`);
        }
    }

    dragShip = null;
}

let lastRotationTime = 0; // Pridaj túto premennú pred funkciu alebo na začiatok súboru

function onDblClick(e) {
    // 1. Debounce (aby to neskákalo príliš rýchlo)
    const now = Date.now();
    if (now - lastRotationTime < 300) return;
    lastRotationTime = now;

    // Povoliť rotáciu iba v správnych fázach
    if (gameState !== 'WAITING_READY' && gameState !== 'PLACING' && gameState !== 'WAITING_OFFSET') return;

    e.preventDefault();

    // Získame bunku, na ktorú sa kliklo
    const cell = e.target.closest('.cell');

    // Ak sme neklikli na loď alebo sme mimo našej mriežky, nič nerobíme
    if (!cell || !cell.classList.contains('ship')) return;
    if (!myGrid.contains(cell)) return;

    // --- TOTO JE TÁ ZMENA PRE OTOČENIE OKOLO MYŠI ---
    if (window.isLocalPlacementMode) {
        // Získame súradnice presne tej kocky, na ktorú si klikol
        const clickX = parseInt(cell.dataset.x);
        const clickY = parseInt(cell.dataset.y);

        // Zavoláme novú funkciu (ktorú si pridal minule) s týmito súradnicami
        handleLocalRotate(clickY, clickX);
    }
}

function updateGhost(e) {
    const x = e.clientX || (e.touches && e.touches[0].clientX);
    const y = e.clientY || (e.touches && e.touches[0].clientY);

    if (x === undefined) return;

    ghostElement.style.display = 'flex';
    ghostElement.style.flexDirection = dragShip.dir === 'H' ? 'row' : 'column';
    ghostElement.innerHTML = '';

    // --- OPRAVA VEĽKOSTI PRE MOBIL ---
    // Namiesto 30px si zmeriame skutočnú šírku prvej bunky v mriežke
    const referenceCell = myGrid.querySelector('.cell');
    // Ak náhodou bunka neexistuje (čo by nemalo nastať), použijeme 30 ako zálohu
    const cellSize = referenceCell ? referenceCell.getBoundingClientRect().width : 30;

    // Nastavenie veľkosti ghost elementu podľa zmeranej bunky
    ghostElement.style.width = (dragShip.dir === 'H' ? dragShip.len * cellSize : cellSize) + 'px';
    ghostElement.style.height = (dragShip.dir === 'V' ? dragShip.len * cellSize : cellSize) + 'px';

    // --- OPRAVA POSUNU (OFFSET) ---
    // Vypočítame spätný posun podľa dynamickej veľkosti
    let shiftX = 0;
    let shiftY = 0;

    if (dragShip.dir === 'H') {
        shiftX = dragShip.offset * cellSize;
    } else {
        shiftY = dragShip.offset * cellSize;
    }

    // Vycentrovanie kurzora: (cellSize / 2) namiesto pevnej 15
    ghostElement.style.left = (x - (cellSize / 2) - shiftX) + 'px';
    ghostElement.style.top = (y - (cellSize / 2) - shiftY) + 'px';

    // Vizuálne štýly
    ghostElement.style.backgroundColor = 'rgba(100, 100, 100, 0.5)';
    ghostElement.style.border = '1px dashed #fff';
    ghostElement.style.pointerEvents = 'none';
}

function markShipCells(r, c, len, dir, cls, remove = false) {
    for (let i = 0; i < len; i++) {
        let xx = c + (dir === 'H' ? i : 0);
        let yy = r + (dir === 'V' ? i : 0);
        const idx = yy * 9 + xx;
        if (myGrid.children[idx]) {
            if (remove) myGrid.children[idx].classList.remove(cls);
            else myGrid.children[idx].classList.add(cls);
        }
    }
}

// Attach listeners
document.addEventListener('mousemove', onMove);
document.addEventListener('mouseup', onUp);
// Touch events
document.addEventListener('touchmove', onMove, {passive: false});
document.addEventListener('touchend', onUp);

// Initialize Grids
function createGrid(element, isInteractive, onClick) {
    element.innerHTML = '';
    // Server uses 9 wide (0-8), 7 high (0-6) based on previous context
    // or standard 10x10?
    // Let's check common macros. STANDARD_WIDTH is usually defined.
    // Wait, let's look at server defines.
    // The previous context "main.c" or "server.c" didn't explicitly show grid size change
    // but typically it's hardcoded or small in this project.
    // Looking at `style.css` I put 9x7. Let's assume 10x10 for safety
    // or whatever the server sends.
    // The server sends `GRID x y ...` so I can just render 10x10 slots.

    // Defaulting to 9x7 to match server defines (GRID_COLS 9, GRID_ROWS 7)
    const W = 9;
    const H = 7;

    // Update CSS grid to match (Use CSS variables now)
    element.style.gridTemplateColumns = `repeat(${W}, var(--cell-size))`;
    element.style.gridTemplateRows = `repeat(${H}, var(--cell-size))`;

    // Disable context menu for right-click rotation
    if(element === myGrid) {
        element.oncontextmenu = (e) => {
            e.preventDefault();
            // Right click triggers rotation logic
            if (gameState === 'PLACING' || gameState === 'WAITING_READY') {
                 onDblClick(e);
            }
            return false;
        };
    }

    for (let y = 0; y < H; y++) {
        for (let x = 0; x < W; x++) {
            const cell = document.createElement('div');
            cell.className = 'cell';
            cell.dataset.x = x;
            cell.dataset.y = y;
            if (isInteractive) {
                cell.onclick = () => onClick(x, y);
                 // Drag start
                if (element === myGrid) {
                    cell.addEventListener('mousedown', onDown);
                    cell.addEventListener('touchstart', onDown, {passive: false});
                    cell.addEventListener('dblclick', onDblClick);
                }
            }
            element.appendChild(cell);
        }
    }
}

// Double tap detection state
let lastClickTime = 0;
let lastClickPos = {x: -1, y: -1};

createGrid(myGrid, true, (x, y) => {
    // Click behavior
    if (gameState === 'PLACING') {
        const now = Date.now();
        const isDoubleTap = (x === lastClickPos.x && y === lastClickPos.y && (now - lastClickTime) < 400);

        lastClickTime = now;
        lastClickPos = {x, y};

        // If Double Tap, trigger rotation explicitly for Mobile
        if (isDoubleTap) {
             const index = y * 9 + x;
             const cell = myGrid.children[index];
             if (cell && cell.classList.contains('ship')) {
                 onDblClick({target: cell, preventDefault:()=>{}});
                 return;
             }
        }

        // Try to place ship
                if (currentShipLen > 0) {
            // 1. Overíme, či sa loď zmestí a neprekrýva (lokálne)
            if (isValidPlacementLocal(y, x, currentShipLen, currentDirection)) {

                // 2. Pridáme loď do lokálneho poľa
                placedShips.push({
                    r: y,
                    c: x,
                    len: currentShipLen,
                    dir: currentDirection
                });

                // 3. Prekreslíme mriežku
                drawAllMyShips();

                // 4. Posunieme sa na ďalšiu loď v poradí
                // (Toto je kľúčové - normálne to robí server cez SHIP_INFO, teraz to musíme urobiť my)
                nextShip();

            } else {
                statusDiv.innerText = "Invalid position!";
                setTimeout(() => statusDiv.innerText = `Place Ship of Length ${currentShipLen}`, 1000);
            }
        } else {
             // We cannot access 'e' here because click handler didn't pass it.
             // We need to find the target cell manually.
             const index = y * 9 + x;
             const cell = myGrid.children[index];
             if (cell && cell.classList.contains('ship')) {
                 onDblClick({target: cell, preventDefault:()=>{}});
             }
        }
    }
});

createGrid(opGrid, true, (x, y) => {
    if (gameState === 'FIRING') {
        // Check if already fired locally
        const idx = y * 9 + x;
        if (opGrid.children[idx] && (opGrid.children[idx].classList.contains('hit') || opGrid.children[idx].classList.contains('miss'))) {
             statusDiv.innerText = "You already shot here!";
             setTimeout(() => {
                 if (gameState === 'FIRING') statusDiv.innerText = "YOUR TURN! Fire at will.";
             }, 1000);
             return;
        }

        // SEND: FIRE ROW COL
        ws.send(`FIRE ${y} ${x}`);
        gameState = 'WAITING_TURN'; // Lock until server says TURN again
        statusDiv.innerText = "Firing...";
    }
});


function handleServerMessage(line) {
    line = line.trim();
    if (!line) return;
    const parts = line.split(' ');
    const cmd = parts[0];

    if (cmd === 'PROXY_HELLO') {
        // Handled in main loop
    } else if (cmd === 'HELLO') {
        statusDiv.innerText = 'Connected to Game Server';
    } else if (cmd === 'LOBBY_LIST_START') {
        tempLobbyList = [];
    } else if (cmd === 'LOBBY') {
        // LOBBY id name num/2 status
        // e.g. LOBBY 0 MyLobby 1/2 OPEN
        tempLobbyList.push({
            id: parts[1],
            name: parts[2], // Simple parsing, user names might have spaces so careful
            count: parts[3],
            status: parts[4]
        });
    } else if (cmd === 'LOBBY_LIST_END') {
        renderLobbies();
    } else if (cmd === 'YOU' || cmd === 'ASSIGN') {
        myPlayerId = parseInt(parts[1]);
        statusDiv.innerHTML = `Joined as Player ${myPlayerId}. Waiting for opponent...`;

        // Add Big Back Button at bottom for Mobile ease
        const container = document.getElementById('game-ui');
        let backBtn = document.getElementById('waiting-back-btn');
        if (!backBtn) {
            backBtn = document.createElement('button');
            backBtn.id = 'waiting-back-btn';
            backBtn.innerText = "Exit to Lobby";
            backBtn.style.backgroundColor = '#d32f2f'; // Red
            backBtn.style.marginTop = '20px';
            backBtn.style.width = '100%';
            backBtn.onclick = () => location.reload();
            container.appendChild(backBtn);
        }
        backBtn.style.display = 'block';

        document.getElementById('setup-screen').style.display = 'none';
        document.getElementById('lobby-screen').style.display = 'none';
        document.getElementById('game-ui').style.display = 'block';
    } else if (cmd === 'START_PLACEMENT') {
        // Expected format: START_PLACEMENT 2 3 3 4 5
        shipsToPlace = parts.slice(1).map(x => parseInt(x));

        // Hide Back Button once game starts
        const backBtn = document.getElementById('waiting-back-btn');
        if (backBtn) backBtn.style.display = 'none';

        nextShip();
    } else if (cmd === 'MOVE_OK') {
        const fromR = parseInt(parts[1]);
        const fromC = parseInt(parts[2]);
        const toR = parseInt(parts[3]);
        const toC = parseInt(parts[4]);
        const dir = parts[5];

        // Remove old ship locally
        // We find it in placedShips and update
                const idx = placedShips.findIndex(s => s.r === fromR && s.c === fromC);
        if (idx !== -1) {
            placedShips[idx].r = toR;
            placedShips[idx].c = toC;
            placedShips[idx].dir = dir;

            // "Nuclear option": Vymaž všetko a prekresli nanovo (zaručene odstráni ghost ships)
            drawAllMyShips();
        } else {
            // Need length? If not in local array, we can't draw easily without length.
            // But we add to local array in drawShip if missing.
            location.reload(); // Fallback if state desync
        }
    } else if (cmd === 'MOVE_FAIL') {
        logToChat("Move failed: " + parts.slice(1).join(' '));
        } else if (cmd === 'PLACED') {
        // PLACED r c len dir ok
        const r = parseInt(parts[1]);
        const c = parseInt(parts[2]);
        const len = parseInt(parts[3]);
        const dir = parts[4];
        const ok = parseInt(parts[5]);

        if (ok === 1) {
            // TU PRIDÁVAME LOĎ DO POĽA (ak tam ešte nie je)
            // Toto je dôležité pre normálne ukladanie (nie cez drag&drop)
            const exists = placedShips.find(s => s.r === r && s.c === c && s.len === len);
            if (!exists) {
                placedShips.push({r, c, len, dir});
            }

            // A potom nakreslíme
            drawShip(myGrid, r, c, len, dir === 'V');
            currentShipLen = 0;
        } else {
             if (isAutoPlacing) {
                 setTimeout(tryRandomPlace, 50);
             } else {
                 statusDiv.innerText = "Invalid Placement!";
                 setTimeout(() => {
                    if(gameState === 'PLACING') statusDiv.innerText = `Place Ship of Length ${currentShipLen}`;
                 }, 1500);
             }
        }
        } else if (cmd === 'SHIP_INFO') {
        // SHIP_INFO r c len
        // NOTE: SHIP_INFO does not contain direction.
        // We rely on PLACED (ok=1) to draw the ship to ensure correct direction.
        // We only use SHIP_INFO to advance the queue and ensure server Sync state.

        // Advance queue
        currentShipLen = 0; // successfully placed this one
        nextShip();
        // If all ships are placed, enable local placement mode
        if (shipsToPlace.length === 0) {
            window.isLocalPlacementMode = true;
        }
    } else if (cmd === 'REVEAL') {
        // REVEAL r c val
        const r = parseInt(parts[1]);
        const c = parseInt(parts[2]);
        // const val = parseInt(parts[3]); // Ship ID, not strictly needed for display

        const index = r * 9 + c;
        if (opGrid.children[index]) {
            const cell = opGrid.children[index];
            // Only add if not already hit/sunk (though server might send all)
            // Visually we want to see the whole ship
            if (!cell.classList.contains('hit') && !cell.classList.contains('sunk')) {
                cell.classList.add('ship');
                cell.classList.add('revealed'); // Optional hook for styling
            }
        }
    } else if (cmd === 'ALL_PLACED') {
        const pid = parseInt(parts[1]);
        // Only show ready button if IT IS ME who finished placing
        if (pid === myPlayerId && !window.isBatchSending) {
            statusDiv.innerText = "All ships placed! Press Ready.";
            createReadyButton();
            // Optional: Switch to My Grid to admire work (already there usually)
        }
    } else if (cmd === 'GRID') {
        // GRID player_id x y char
        // E.g. GRID 0 1 1 S
        const pid = parseInt(parts[1]);
        const x = parseInt(parts[2]);
        const y = parseInt(parts[3]);
        const type = parts[4];

        // Ensure within bounds (visual)
        if (x >= 9 || y >= 7) return; // Ignore OOB for visual

        const grid = (pid === myPlayerId) ? myGrid : opGrid;
        const index = y * 9 + x;
        const cell = grid.children[index];

        if (cell) {
            cell.className = 'cell'; // Reset
            if (type === 'S') cell.classList.add('ship');
            else if (type === 'H') cell.classList.add('hit');
            else if (type === 'M') cell.classList.add('miss');
            else if (type === 'K') cell.classList.add('sunk');
        }
    } else if (cmd === 'TURN') {
        // TURN <player_id>
        const turnPid = parseInt(parts[1]);
        if (turnPid === myPlayerId) {
            gameState = 'FIRING';
            statusDiv.innerText = "YOUR TURN! Fire at will.";
            document.getElementById('place-controls').style.display = 'none';
            document.getElementById('fire-controls').style.display = 'block';

            // Auto-switch to Targeting on Mobile
            switchTab('op-grid');
        } else {
            gameState = 'WAITING_TURN';
            statusDiv.innerText = "Opponent's Turn";
            document.getElementById('place-controls').style.display = 'none';
            document.getElementById('fire-controls').style.display = 'none';

            // Auto-switch to My Feet to see hits on Mobile
            switchTab('my-grid');
        }
    } else if (cmd === 'WAIT') {
        gameState = 'WAITING_TURN';
        statusDiv.innerText = "Waiting for opponent...";
        document.getElementById('place-controls').style.display = 'none';
        document.getElementById('fire-controls').style.display = 'none';
    } else if (cmd === 'WIN') {
        gameState = 'GAMEOVER';
        statusDiv.innerText = "YOU WIN!";
        logToChat("VICTORY! - You won the game.", '#4CAF50');
    } else if (cmd === 'LOSE') {
        gameState = 'GAMEOVER';
        statusDiv.innerText = "YOU LOSE!";
        logToChat("DEFEAT! - You lost the game.", '#f44336');
    } else if (cmd === 'PLAY_AGAIN') {
        statusDiv.innerText = "Game Over. Play Again?";
        createPlayAgainButtons();
    } else if (cmd === 'RESTART_GAME') {
         // Server sends this when both say YES
        logToChat("Both players accepted! Restarting...");
        // Reset local state
        resetGameUI();
        // Server will send READY or PLACING sequence next?
        // Actually server usually resets game state.
        // We need to clear grids visually.
    } else if (cmd === 'MESSAGE') {
        // Only if addressed to me?
        // Actually usually chat. But let's check format.
        // Server sends "MESSAGE <text>".
        // If it sends "MESSAGE <text>" to both, both see it.
        const msg = parts.slice(1).join(' ');
        logToChat(msg);
    } else if (cmd === 'FIRE_ACK') {
        // FIRE_ACK r c hit
        const r = parseInt(parts[1]);
        const c = parseInt(parts[2]);
        const hit = parseInt(parts[3]);

        // Update my view of opponent's grid
        const index = r * 10 + c; // Still using 10 width for index logic since row is y?
        // Wait, earlier createGrid loop uses W=9.
        // let's double check W.
        // server uses W=9.
        const index9 = r * 9 + c;
        if (index9 >= 0 && index9 < opGrid.children.length) { // fixed check
             const cell = opGrid.children[index9];
             if (hit) cell.classList.add('hit');
             else cell.classList.add('miss');
        }
        statusDiv.innerText = hit ? "HIT!" : "MISS!";
    } else if (cmd === 'RESULT') {
        // RESULT r c hit (Opponent fired at me)
        const r = parseInt(parts[1]);
        const c = parseInt(parts[2]);
        const hit = parseInt(parts[3]);

        // Update my own grid
        const index9 = r * 9 + c;
        if (index9 >= 0 && index9 < myGrid.children.length) {
             const cell = myGrid.children[index9];
             if (hit) cell.classList.add('hit');
             else cell.classList.add('miss'); // opponent missed me
        }
    } else if (cmd === 'SHIP_SUNK') {
        const victim = parseInt(parts[1]);
        const len = parseInt(parts[2]);
        if (victim === myPlayerId) {
             logToChat(`They sunk your ship (len ${len})!`, '#f44336');
        } else {
             logToChat(`You sunk their ship (len ${len})!`, '#4CAF50');
        }
    } else if (cmd === 'OPPONENT_DISCONNECTED') {
        // Hard disconnection handling
        statusDiv.innerText = "Opponent Disconnected! Returning to lobby...";
        logToChat("Opponent Disconnected! Refreshing in 3s...");
        setTimeout(() => location.reload(), 3000);
    } else if (cmd === 'OPPONENT_LEFT') {
        const pab = document.getElementById('play-again-box');
        // Only treat as HOST RECOVERY if mid-game (no play again box visible)
        // Actually, if we are in GAMEOVER state, and opponent left, we should prob go to lobby too?
        // But user asked strictly "if one player quits [at end]... lobby full... move player to lobby screen".
        // The server sends GAME_CLOSED for that specific case now.
        // So OPPONENT_LEFT is reserved for Mid-Game disconnects (or waiting phase disconnects).

        statusDiv.innerHTML = "Opponent left. Waiting for new player...";
        logToChat("Opponent left the game. You are now the host waiting for a challenger.");

        // Show Big Red Back Button
        const container = document.getElementById('game-ui');
        let backBtn = document.getElementById('waiting-back-btn');
        if (!backBtn) {
            backBtn = document.createElement('button');
            backBtn.id = 'waiting-back-btn';
            backBtn.innerText = "Exit to Lobby";
            backBtn.style.backgroundColor = '#d32f2f'; // Red
            backBtn.style.marginTop = '20px';
            backBtn.style.width = '100%';
            backBtn.onclick = () => location.reload();
            container.appendChild(backBtn);
        }
        backBtn.style.display = 'block';

        // Reset local state
        resetGameUI();
        if (pab) pab.remove();
        document.getElementById('place-controls').style.display = 'none';

    } else if (cmd === 'GAME_CLOSED') {
        // Opponent refused rematch
        alert("Opponent left/declined. Returning to lobby.");
        location.reload();
    } else if (cmd === 'LOBBY_LIST_END') {
        renderLobbies();
    } else if (cmd === 'RESTART') {
        // Reset Game State for Rematch
        // Similar to initial setup
        alert("Restarting Game...");
        location.reload();
    } else if (cmd === 'BUSY') {
        alert("Server is full or busy.");
    } else if (cmd === 'ALREADY_FIRED') {
        logToChat("You already fired there!", '#ff9800');
        statusDiv.innerText = "Already fired there! Try again.";
        gameState = 'FIRING';
    } else if (cmd === 'NOT_YOUR_TURN') {
        alert("It is not your turn!");
    } else if (cmd === 'NOT_READY') {
        alert("Game not ready yet.");
    } else if (cmd === 'PLAYER') {
        const pid = parseInt(parts[1]);
        if (parts[2] === 'PLACED') {
            const len = parseInt(parts[3]);
            if (pid !== myPlayerId) {
                // If opponent placed a ship, maybe show a notification?
                // Does NOT effect my state
                const p = document.createElement('div');
                p.innerText = `Opponent placed a ship (len ${len})`;
                p.style.color = '#aaa';
                p.style.fontStyle = 'italic';
                messagesDiv.appendChild(p);
            }
        }
    }
}

function joinGame() {
    // Legacy function - kept just in case but auto-join handles it now
    const name = "Player";
    ws.send(`NAME ${name}`);
    gameState = 'LOBBY_SELECT';
    statusDiv.innerText = "Select a Lobby...";
    document.getElementById('setup-screen').style.display = 'none';
    document.getElementById('lobby-screen').style.display = 'block';
}

function refreshLobbies() {
    ws.send('LOBBY_LIST');
}

function createLobby() {
    const name = document.getElementById('lobby-name').value || "MyLobby";
    ws.send(`LOBBY_CREATE ${name}`);
}

function joinLobby(id) {
    ws.send(`LOBBY_JOIN ${id}`);
}

function renderLobbies() {
    const list = document.getElementById('lobby-list');
    list.innerHTML = '';

    if (tempLobbyList.length === 0) {
        list.innerHTML = '<p>No lobbies found. Create one!</p>';
        return;
    }

    tempLobbyList.forEach(l => {
        const div = document.createElement('div');
        div.className = 'lobby-item';

        const info = document.createElement('span');
        info.innerText = `${l.name} (${l.count})`;

        const btn = document.createElement('button');
        if (l.status === 'LOCKED') {
            btn.className = 'lobby-btn-locked';
            btn.innerText = 'Full';
            btn.disabled = true;
        } else {
            btn.className = 'lobby-btn-join';
            btn.innerText = 'Join';
            btn.onclick = () => joinLobby(l.id);
        }

        div.appendChild(info);
        div.appendChild(btn);
        list.appendChild(div);
    });
}

function createPlayAgainButtons() {
    const container = document.getElementById('controls');
    // NEVYMAZÁVAŤ container.innerHTML = '', lebo by si si zmazal aj iné ovládacie prvky!
    // Namiesto toho len skontrolujeme, či tam už box nie je
    if (document.getElementById('play-again-box')) return;

    const div = document.createElement('div');
    div.id = 'play-again-box'; // <--- TOTO TAM CHÝBALO!
    div.style.textAlign = 'center';
    div.style.padding = '20px';

    const btnYes = document.createElement('button');
    btnYes.innerText = 'Play Again';
    btnYes.style.backgroundColor = '#2ecc71';
    btnYes.style.marginRight = '10px';
    btnYes.onclick = () => {
        ws.send('PLAY_AGAIN YES');
        // Zmeníme text len vo vnútri tohto boxu
        div.innerText = 'Waiting for opponent response...';
    };

    const btnNo = document.createElement('button');
    btnNo.innerText = 'Quit';
    btnNo.style.backgroundColor = '#e74c3c';
    btnNo.onclick = () => {
        ws.send('PLAY_AGAIN NO');
        location.reload();
    };

    div.appendChild(btnYes);
    div.appendChild(btnNo);
    container.appendChild(div);
}

function setDir(d) {
    currentDirection = d;
    document.getElementById('current-dir').innerText = d;
}

function startAutoPlace() {
    window.isLocalPlacementMode = true;

    currentShipLen = 0;
    statusDiv.innerText = "Ships placed randomly. Adjust or click Ready.";
    // Reset local/visual state
    placedShips = [];
    shipsToPlace = [5, 4, 3, 3, 2];

    // Clear Grid Visuals
    document.querySelectorAll('.cell.ship').forEach(el => el.classList.remove('ship'));
    if (document.getElementById('ready-btn')) document.getElementById('ready-btn').remove();

    gameState = 'PLACING';

    // Place all ships locally
    // We strive to place all. If we fail, we restart (simple retry)
    // To prevent infinite recursion, we'll try a loop here.
    let success = false;
    let attempts = 0;

    while (!success && attempts < 50) {
        // Soft reset
        placedShips = [];
        shipsToPlace = [5, 4, 3, 3, 2];
        let currentSuccess = true;

        // Temp copy of ships needed
        const currentQueue = [...shipsToPlace];

        for (const len of currentQueue) {
            if (!tryRandomPlaceLocal(len)) {
                currentSuccess = false;
                break;
            }
        }

        if (currentSuccess) {
            success = true;
        }
        attempts++;
    }

    if (success) {
        // Draw all
        document.querySelectorAll('.cell.ship').forEach(el => el.classList.remove('ship'));
        placedShips.forEach(s => {
            drawShip(myGrid, s.r, s.c, s.len, s.dir === 'V');
        });
        createReadyButton(true);
    } else {
        console.error("Could not find valid placement after multiple attempts");
        statusDiv.innerText = "Auto-placement failed. Try again.";
    }
}

function tryRandomPlaceLocal(len) {
    let placed = false;
    let attempts = 0;
    while (!placed && attempts < 200) {
        const r = Math.floor(Math.random() * 7);
        const c = Math.floor(Math.random() * 9);
        const dir = Math.random() > 0.5 ? 'H' : 'V';

        if (isValidPlacementLocal(r, c, len, dir)) {
            placedShips.push({r, c, len, dir});
            placed = true;
            return true;
        }
        attempts++;
    }
    return false;
}

// Pridal som parameter 'ignoreIndex' (defaultne -1, čiže kontroluje všetko)
function isValidPlacementLocal(r, c, len, dir, ignoreIndex = -1) {
    // 1. Kontrola hraníc (zostáva rovnaká)
    if (dir === 'H') {
        if (c + len > 9) return false;
    } else {
        if (r + len > 7) return false;
    }

    // 2. Kontrola kolízií
    let newShipCells = [];
    for (let i = 0; i < len; i++) {
        newShipCells.push({
            r: r + (dir === 'V' ? i : 0),
            c: c + (dir === 'H' ? i : 0)
        });
    }

    // ZMENA: Používame klasický for cyklus, aby sme mali prístup k indexu 'k'
    for (let k = 0; k < placedShips.length; k++) {
        // Ak práve kontrolujeme tú istú loď, ktorú presúvame, preskoč ju
        if (k === ignoreIndex) continue;

        let ship = placedShips[k];

        for (let i = 0; i < ship.len; i++) {
            let existingR = ship.r + (ship.dir === 'V' ? i : 0);
            let existingC = ship.c + (ship.dir === 'H' ? i : 0);

            for (let newCell of newShipCells) {
                if (newCell.r === existingR && newCell.c === existingC) {
                    return false; // Kolízia s inou loďou!
                }
            }
        }
    }
    return true;
}

function createReadyButton(isBatchSend = false) {
    // Check if button already exists
    if (document.getElementById('ready-btn')) return;

    const btn = document.createElement('button');
    btn.id = 'ready-btn';
    btn.innerText = isBatchSend ? "Ready (Send Ships)" : "I'm Ready!";
    btn.style.backgroundColor = '#2ecc71';
    btn.style.color = 'white';
    btn.style.padding = '10px 20px';
    btn.style.fontSize = '16px';
    btn.style.marginTop = '10px';
    btn.style.border = 'none';
    btn.style.borderRadius = '5px';
    btn.style.cursor = 'pointer';

    btn.onclick = () => {
        if (isBatchSend) {
             window.isBatchSending = true; // Block UI updates
             statusDiv.innerText = "Sending ships to server...";
             btn.disabled = true;

             // Disable controls
             const pc = document.getElementById('place-controls');
             if(pc) pc.style.display = 'none';

             // Batch send with delay
             placedShips.forEach((s, i) => {
                 setTimeout(() => {
                     ws.send(`PLACE ${s.r} ${s.c} ${s.len} ${s.dir}`);
                     if (i === placedShips.length - 1) {
                         // Last ship sent, send ready
                         setTimeout(() => {
                             ws.send('READY');
                             btn.remove();
                             statusDiv.innerText = "Waiting for other players...";
                             gameState = 'WAITING_READY';
                             window.isBatchSending = false; // Release lock
                         }, 300);
                     }
                 }, i * 150);
             });
        } else {
             ws.send('READY');
             btn.remove();
             statusDiv.innerText = "Waiting for other players...";
             gameState = 'WAITING_READY';
        }
    };

    // Add to controls container, so it's visible even if place-controls is hidden
    document.getElementById('controls').appendChild(btn);
}

/* Helper functions for Local Placement */

function handleLocalMove(fromR, fromC, toR, toC, dir, len) {
    // 1. Skontroluj, či cieľová pozícia nie je mimo mapy
    let validBounds = false;
    if (dir === 'H') {
        if (toC >= 0 && toC + len <= 9 && toR >= 0 && toR < 7) validBounds = true;
    } else {
        if (toR >= 0 && toR + len <= 7 && toC >= 0 && toC < 9) validBounds = true;
    }

    if (!validBounds) {
        logToChat("Cannot move: Out of bounds");
        drawAllMyShips(); // Reset vizuálu (vráti loď naspäť)
        return;
    }

    // 2. Dočasne odstráň starú loď z poľa, aby nezavadzala pri kontrole kolízií
    const shipIndex = placedShips.findIndex(s => s.r === fromR && s.c === fromC);
    if (shipIndex === -1) {
        drawAllMyShips();
        return;
    }

    // Odložíme si loď bokom
    const tempShip = placedShips.splice(shipIndex, 1)[0];

    // 3. Skontroluj kolízie s ostatnými loďami
    let collision = false;
    for (let i = 0; i < len; i++) {
        let x = toC + (dir === 'H' ? i : 0);
        let y = toR + (dir === 'V' ? i : 0);

        for (const other of placedShips) {
            // Skontroluj prekrytie s 'other' loďou
            for (let j = 0; j < other.len; j++) {
                let ox = other.c + (other.dir === 'H' ? j : 0);
                let oy = other.r + (other.dir === 'V' ? j : 0);
                if (x === ox && y === oy) collision = true;
            }
        }
    }

    if (collision) {
        logToChat("Cannot move: Collision");
        // Vráť starú loď naspäť na pôvodné miesto
        placedShips.push(tempShip);
    } else {
        // 4. Úspešný presun - ulož novú pozíciu
        placedShips.push({ r: toR, c: toC, len: len, dir: dir });
    }

    // 5. Vždy prekresli všetko, aby zmizli duchovia
    drawAllMyShips();
}

function handleLocalRotate(clickR, clickC) {
    // 1. Nájdi loď, na ktorú si klikol
    const shipIndex = placedShips.findIndex(s => {
        if (s.dir === 'H') {
            return s.r === clickR && clickC >= s.c && clickC < s.c + s.len;
        } else {
            return s.c === clickC && clickR >= s.r && clickR < s.r + s.len;
        }
    });

    if (shipIndex === -1) return; // Klikol si mimo lode

    const ship = placedShips[shipIndex];

    // 2. Vypočítaj "Pivot" (index dielika, na ktorý si klikol: 0, 1, 2...)
    // Ak je loď H, rozdiel je v stĺpcoch. Ak V, rozdiel je v riadkoch.
    const offset = (ship.dir === 'H')
                   ? (clickC - ship.c)
                   : (clickR - ship.r);

    // 3. Priprav nové súradnice
    const newDir = (ship.dir === 'H') ? 'V' : 'H';
    let newR, newC;

    if (newDir === 'V') {
        // Meníme na Vertikálnu:
        // Kliknutý bod (clickR, clickC) musí byť na pozícii 'offset' v novej lodi
        newR = clickR - offset;
        newC = clickC;
    } else {
        // Meníme na Horizontálnu:
        newR = clickR;
        newC = clickC - offset;
    }

    // 4. "Wall Kick" (Ochrana proti vylezeniu z mapy)
    // Ak otáčanie spôsobí, že loď vylezie von, posunieme ju späť.

    // a) Oprava hraníc pre riadky (0 až 7)
    if (newR < 0) newR = 0;
    if (newR + ship.len > 8) newR = 8 - ship.len; // 8 je výška mapy

    // b) Oprava hraníc pre stĺpce (0 až 9)
    if (newC < 0) newC = 0;
    if (newC + ship.len > 10) newC = 10 - ship.len; // 10 je šírka mapy

    // 5. Validácia a uloženie
    // Najprv "vyberieme" starú loď, aby nezavadzala pri kontrole
    placedShips.splice(shipIndex, 1);

    // Skontrolujeme, či na novom mieste nekoliduje s inou loďou
    // (Funkciu isValidPlacementLocal už máš opravenú z minula)
    if (isValidPlacementLocal(newR, newC, ship.len, newDir)) {
        // Je to OK, vložíme otočenú loď
        placedShips.push({
            r: newR,
            c: newC,
            len: ship.len,
            dir: newDir
        });
        drawAllMyShips(); // Prekresliť
    } else {
        // Kolízia! Vrátime starú loď tam, kde bola (rotácia zlyhala)
        placedShips.push(ship);

        statusDiv.innerText = "Cannot Rotate here!";
        setTimeout(() => {
             if (window.isLocalPlacementMode) statusDiv.innerText = "Arrange your ships...";
        }, 1000);
    }
}

function isValidPlacementLocalWithList(r, c, len, dir, list) {
    if (dir === 'H') {
        if (c < 0 || c + len > 9) return false;
        if (r < 0 || r >= 7) return false;
    } else {
        if (r < 0 || r + len > 7) return false;
        if (c < 0 || c >= 9) return false;
    }

    for (let i = 0; i < len; i++) {
        let x = c + (dir === 'H' ? i : 0);
        let y = r + (dir === 'V' ? i : 0);

        for (let s of list) {
            for (let j = 0; j < s.len; j++) {
                let sx = s.c + (s.dir === 'H' ? j : 0);
                let sy = s.r + (s.dir === 'V' ? j : 0);
                if (x === sx && y === sy) return false;
            }
        }
    }
    return true;
}

function drawAllMyShips() {
    // Clear my grid ships
    document.querySelectorAll('#my-grid .cell').forEach(c => {
        c.classList.remove('ship');
        c.classList.remove('dragging-source');
        delete c.dataset.shipR;
        delete c.dataset.shipC;
        delete c.dataset.shipLen;
        delete c.dataset.shipDir;
    });

    placedShips.forEach(s => {
        drawShip(myGrid, s.r, s.c, s.len, s.dir === 'V');
    });
}