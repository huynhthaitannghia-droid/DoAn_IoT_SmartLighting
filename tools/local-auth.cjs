const { createHash, randomBytes, timingSafeEqual } = require('node:crypto');
const { SignJWT, jwtVerify } = require('jose');

const issuer = 'smartlight-local';
const audience = 'smartlight-dashboard';
const cookieName = 'smartlight_session';
const lifetime = 30 * 60;
const sessions = new Map();
const signingKey = randomBytes(32);
const generatedPassword = process.env.SMARTLIGHT_PASSWORD ? null : randomBytes(18).toString('base64url');
const passwordDigest = digest(process.env.SMARTLIGHT_PASSWORD || generatedPassword);
let failures = 0;
let failureWindowEnd = 0;

function digest(value) {
    return createHash('sha256').update(value).digest();
}

function endSession(session) {
    sessions.delete(session.id);
    clearTimeout(session.timer);
    for (const socket of session.sockets) socket.terminate();
}

async function verifySession(request) {
    const cookie = request.headers.cookie?.split(';').map(part => part.trim())
        .find(part => part.startsWith(`${cookieName}=`));
    if (!cookie) return null;
    try {
        const { payload } = await jwtVerify(cookie.slice(cookieName.length + 1), signingKey, {
            issuer, audience, algorithms: ['HS256'], requiredClaims: ['exp', 'sub', 'jti']
        });
        return payload.sub === 'admin' ? sessions.get(payload.jti) || null : null;
    } catch {
        return null;
    }
}

function redirect(response, location, cookie) {
    const headers = { Location: location, 'Cache-Control': 'no-store' };
    if (cookie) headers['Set-Cookie'] = cookie;
    response.writeHead(303, headers).end();
}

function cookie(value, maxAge) {
    // HTTP on loopback for the local demo. HTTPS deployments should also set Secure.
    return `${cookieName}=${value}; Path=/; HttpOnly; SameSite=Strict; Max-Age=${maxAge}`;
}

function hasLocalHost(request) {
    return /^(?:localhost|127\.0\.0\.1)(?::\d{1,5})?$/.test(request.headers.host || '');
}

function isSameOrigin(request) {
    return hasLocalHost(request) && request.headers.origin === `http://${request.headers.host}`;
}

async function login(request, response) {
    if (!isSameOrigin(request)) return response.writeHead(403).end();
    if (!/^application\/x-www-form-urlencoded(?:\s*;|$)/i.test(request.headers['content-type'] || '')) {
        return response.writeHead(415).end();
    }

    let body = '';
    request.setEncoding('utf8');
    for await (const chunk of request) {
        body += chunk;
        if (body.length > 2048) return response.writeHead(413).end();
    }

    if (Date.now() >= failureWindowEnd) {
        failures = 0;
        failureWindowEnd = Date.now() + 60000;
    }
    if (failures >= 8) return redirect(response, '/login.html?error=limit');

    const password = new URLSearchParams(body).get('password') || '';
    if (!timingSafeEqual(digest(password), passwordDigest)) {
        failures++;
        return redirect(response, '/login.html?error=password');
    }
    failures = 0;

    const previous = await verifySession(request);
    if (previous) endSession(previous);
    const id = randomBytes(16).toString('hex');
    const token = await new SignJWT({}).setProtectedHeader({ alg: 'HS256', typ: 'JWT' })
        .setIssuer(issuer).setAudience(audience).setSubject('admin').setJti(id)
        .setIssuedAt().setExpirationTime(`${lifetime}s`).sign(signingKey);
    const session = { id, sockets: new Set(), timer: null };
    session.timer = setTimeout(() => endSession(session), lifetime * 1000).unref();
    sessions.set(id, session);
    redirect(response, '/local.html', cookie(token, lifetime));
}

async function logout(request, response) {
    if (!isSameOrigin(request)) return response.writeHead(403).end();
    const session = await verifySession(request);
    if (session) endSession(session);
    redirect(response, '/login.html', cookie('', 0));
}

module.exports = { generatedPassword, hasLocalHost, isSameOrigin, verifySession, login, logout, redirect };
