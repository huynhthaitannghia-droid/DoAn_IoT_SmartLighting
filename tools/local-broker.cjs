const { createServer: createHttpServer } = require('node:http');
const { createServer: createTcpServer } = require('node:net');
const { createReadStream, mkdirSync } = require('node:fs');
const { dirname, resolve } = require('node:path');
const { DatabaseSync } = require('node:sqlite');
const broker = require('aedes')();
const { WebSocketServer, createWebSocketStream } = require('ws');
const auth = require('./local-auth.cjs');

const webHost = '127.0.0.1';
const webPort = Number(process.env.SMARTLIGHT_WEB_PORT || 8080);
const mqttPort = Number(process.env.SMARTLIGHT_MQTT_PORT || 1883);
const dashboard = resolve(__dirname, '../dashboard.html');
const loginPage = resolve(__dirname, 'login.html');
const mqttScript = require.resolve('mqtt/dist/mqtt.min');
const controlTopic = 'nhom15/smartlight/control';
const statusTopic = 'nhom15/smartlight/status';
const ackTopic = 'nhom15/smartlight/ack';
const dashboardStreams = new WeakSet();

broker.preConnect = (client, packet, callback) => {
    const dashboardClient = dashboardStreams.has(client.conn);
    const dashboardId = typeof packet.clientId === 'string' && packet.clientId.startsWith('WebDashboard_Nhom15_');
    callback(null, dashboardClient === dashboardId && !packet.will);
};

broker.authorizePublish = (client, packet, callback) => {
    const allowed = dashboardStreams.has(client.conn)
        ? packet.topic === controlTopic && !packet.retain
        : packet.topic === statusTopic || packet.topic === ackTopic;
    callback(allowed ? null : new Error('MQTT topic not permitted'));
};

broker.authorizeSubscribe = (client, subscription, callback) => {
    const { topic } = subscription;
    const allowed = dashboardStreams.has(client.conn)
        ? topic === statusTopic || topic === ackTopic
        : topic === controlTopic;
    callback(null, allowed ? subscription : null);
};
const historyPath = process.env.SMARTLIGHT_HISTORY_DB || resolve(__dirname, '../data/smartlight-history.sqlite');
if (historyPath !== ':memory:') mkdirSync(dirname(historyPath), { recursive: true });
const history = new DatabaseSync(historyPath);
history.exec(`
    CREATE TABLE IF NOT EXISTS light_history (
        id INTEGER PRIMARY KEY,
        recorded_at TEXT NOT NULL,
        state TEXT NOT NULL CHECK (state IN ('ON', 'OFF')),
        source TEXT,
        brightness INTEGER,
        initial INTEGER NOT NULL CHECK (initial IN (0, 1))
    )
`);
const readHistory = history.prepare('SELECT * FROM light_history ORDER BY id DESC LIMIT 20');
const latestState = history.prepare('SELECT state FROM light_history ORDER BY id DESC LIMIT 1');
const insertHistory = history.prepare(`
    INSERT INTO light_history (recorded_at, state, source, brightness, initial)
    VALUES (?, ?, ?, ?, ?)
`);

broker.on('publish', (packet, client) => {
    if (!client || packet.topic !== statusTopic || packet.payload.length > 1024) return;

    let status;
    try {
        status = JSON.parse(packet.payload.toString('utf8'));
    } catch {
        return;
    }
    if (!status || (status.state !== 'ON' && status.state !== 'OFF') || status.conflict) return;

    try {
        const previous = latestState.get();
        if (previous?.state === status.state) return;
        const source = typeof status.source === 'string' ? status.source.trim().slice(0, 120) : null;
        const brightness = Number.isInteger(status.brightness) && status.brightness >= 0 && status.brightness <= 100
            ? status.brightness : null;
        insertHistory.run(new Date().toISOString(), status.state, source, brightness, previous ? 0 : 1);
    } catch (error) {
        console.error('Không lưu được lịch sử đèn:', error.message);
    }
});

function serveFile(response, path, contentType) {
    const file = createReadStream(path);
    file.once('open', () => {
        response.setHeader('Content-Type', contentType);
        file.pipe(response);
    });
    file.once('error', () => {
        if (response.headersSent) response.destroy();
        else response.writeHead(500).end();
    });
}

async function handleHttp(request, response) {
    if (!auth.hasLocalHost(request)) return response.writeHead(403).end();
    const path = request.url.split('?')[0];
    if (request.method === 'POST' && path === '/login') return auth.login(request, response);
    if (request.method === 'POST' && path === '/logout') return auth.logout(request, response);
    if (request.method !== 'GET') {
        response.writeHead(405).end();
    } else if (path === '/') {
        auth.redirect(response, '/local.html');
    } else if (path === '/login.html') {
        response.setHeader('Cache-Control', 'no-store');
        serveFile(response, loginPage, 'text/html; charset=utf-8');
    } else if (path === '/local.html') {
        if (!await auth.verifySession(request)) return auth.redirect(response, '/login.html');
        response.setHeader('Cache-Control', 'no-store');
        serveFile(response, dashboard, 'text/html; charset=utf-8');
    } else if (path === '/mqtt.min.js') {
        serveFile(response, mqttScript, 'text/javascript; charset=utf-8');
    } else if (path === '/api/history') {
        if (!await auth.verifySession(request)) return response.writeHead(401, { 'Cache-Control': 'no-store' }).end();
        try {
            const entries = readHistory.all();
            response.writeHead(200, {
                'Content-Type': 'application/json; charset=utf-8',
                'Cache-Control': 'no-store'
            }).end(JSON.stringify(entries));
        } catch (error) {
            console.error('Không đọc được lịch sử đèn:', error.message);
            response.writeHead(500).end();
        }
    } else {
        response.writeHead(404).end();
    }
}

const httpServer = createHttpServer((request, response) => {
    handleHttp(request, response).catch(error => {
        console.error('Lỗi dashboard nội bộ:', error.message);
        if (!response.headersSent) response.writeHead(500).end();
        else response.destroy();
    });
});
httpServer.on('error', reportServerError);

const webSockets = new WebSocketServer({ noServer: true, path: '/mqtt' });
webSockets.on('error', reportServerError);
httpServer.on('upgrade', async (request, socket, head) => {
    const swallowSocketError = () => socket.destroy();
    socket.on('error', swallowSocketError);
    if (request.url !== '/mqtt' || !auth.isSameOrigin(request)) {
        socket.end('HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n');
        return;
    }
    const session = await auth.verifySession(request);
    if (!session) {
        socket.end('HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n');
        return;
    }
    if (socket.destroyed) return;
    socket.off('error', swallowSocketError);
    webSockets.handleUpgrade(request, socket, head, webSocket => {
        session.sockets.add(webSocket);
        webSocket.once('close', () => session.sockets.delete(webSocket));
        const stream = createWebSocketStream(webSocket);
        dashboardStreams.add(stream);
        stream.on('error', () => stream.destroy());
        broker.handle(stream);
    });
});

const tcpServer = createTcpServer(broker.handle);
function reportServerError(error) {
    console.error(`Không khởi động được broker nội bộ: ${error.message}`);
    process.exit(1);
}
tcpServer.on('error', reportServerError);

broker.on('clientReady', client => console.log(`MQTT connected: ${client.id}`));
tcpServer.listen(mqttPort, '127.0.0.1', () => console.log(`MQTT TCP: 127.0.0.1:${tcpServer.address().port}`));
httpServer.listen(webPort, webHost, () => {
    console.log(`Dashboard: http://${webHost}:${httpServer.address().port}/`);
    if (auth.generatedPassword) console.log(`Mật khẩu dashboard cho phiên chạy này: ${auth.generatedPassword}`);
});
