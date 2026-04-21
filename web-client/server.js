#!/usr/bin/env node

/**
 * FreeRDP Web Proxy Server
 * Bridges WebSocket connections to RDP for web-based remote desktop
 */

const WebSocket = require('ws');
const express = require('express');
const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');

class FreeRDPWebProxy {
    constructor(options = {}) {
        this.port = options.port || 8080;
        this.rdpHost = options.rdpHost || 'localhost';
        this.rdpPort = options.rdpPort || 3389;
        this.width = options.width || 1920;
        this.height = options.height || 1080;
        this.fps = options.fps || 60;

        this.app = express();
        this.wss = null;
        this.clients = new Map();
        this.shadowProcess = null;

        this.setupExpress();
        this.setupWebSocket();
    }

    setupExpress() {
        // Serve static files
        this.app.use(express.static(path.join(__dirname)));

        // Health check endpoint
        this.app.get('/health', (req, res) => {
            res.json({
                status: 'ok',
                clients: this.clients.size,
                shadowRunning: this.shadowProcess !== null
            });
        });

        this.server = this.app.listen(this.port, () => {
            console.log(`FreeRDP Web Proxy listening on port ${this.port}`);
            console.log(`RDP server: ${this.rdpHost}:${this.rdpPort}`);
            console.log(`Target resolution: ${this.width}x${this.height} @ ${this.fps}fps`);
        });
    }

    setupWebSocket() {
        this.wss = new WebSocket.Server({ server: this.server, path: '/rdp' });

        this.wss.on('connection', (ws, req) => {
            const clientId = this.generateClientId();
            console.log(`New WebSocket connection: ${clientId}`);

            const client = {
                id: clientId,
                ws: ws,
                connected: false,
                rdpConnection: null
            };

            this.clients.set(clientId, client);
            this.handleClient(client);

            ws.on('close', () => {
                console.log(`Client disconnected: ${clientId}`);
                this.cleanupClient(client);
            });

            ws.on('error', (error) => {
                console.error(`WebSocket error for ${clientId}:`, error);
                this.cleanupClient(client);
            });
        });
    }

    handleClient(client) {
        client.ws.on('message', (data) => {
            try {
                const message = JSON.parse(data.toString());

                switch (message.type) {
                    case 'connect':
                        this.connectRDP(client, message);
                        break;
                    case 'mouse':
                        this.handleMouse(client, message);
                        break;
                    case 'keyboard':
                        this.handleKeyboard(client, message);
                        break;
                    default:
                        console.log(`Unknown message type: ${message.type}`);
                }
            } catch (error) {
                console.error('Error parsing message:', error);
            }
        });

        // Send initial connection info
        client.ws.send(JSON.stringify({
            type: 'connected',
            width: this.width,
            height: this.height
        }));
    }

    connectRDP(client, message) {
        console.log(`Connecting RDP for client ${client.id}`);

        // For now, we'll simulate RDP connection
        // In a real implementation, this would connect to FreeRDP shadow server
        client.connected = true;

        // Start screen capture simulation
        this.startScreenCapture(client);

        client.ws.send(JSON.stringify({
            type: 'rdp_connected',
            message: 'Connected to RDP server'
        }));
    }

    startScreenCapture(client) {
        // For demonstration, we'll create a simple screen capture using ffmpeg
        // In production, this would integrate with FreeRDP's shadow capture

        const ffmpeg = spawn('ffmpeg', [
            '-f', 'gdigrab',
            '-framerate', this.fps.toString(),
            '-video_size', `${this.width}x${this.height}`,
            '-i', 'desktop',
            '-f', 'mjpeg',
            '-q:v', '2',
            '-r', this.fps.toString(),
            'pipe:1'
        ]);

        client.screenCapture = ffmpeg;

        ffmpeg.stdout.on('data', (data) => {
            if (client.ws.readyState === WebSocket.OPEN) {
                // Send frame data to client
                client.ws.send(data, { binary: true });
            }
        });

        ffmpeg.stderr.on('data', (data) => {
            console.log(`FFmpeg: ${data}`);
        });

        ffmpeg.on('close', (code) => {
            console.log(`Screen capture stopped for client ${client.id}, code: ${code}`);
        });

        console.log(`Started screen capture for client ${client.id}`);
    }

    handleMouse(client, message) {
        if (!client.connected) return;

        // In a real implementation, this would send mouse events to RDP
        console.log(`Mouse ${message.event}: ${message.x}, ${message.y}, button: ${message.button}`);

        // For now, we'll just acknowledge
        // TODO: Send to RDP server
    }

    handleKeyboard(client, message) {
        if (!client.connected) return;

        // In a real implementation, this would send keyboard events to RDP
        console.log(`Keyboard ${message.event}: keyCode=${message.keyCode}, ctrl=${message.ctrlKey}`);

        // For now, we'll just acknowledge
        // TODO: Send to RDP server
    }

    cleanupClient(client) {
        if (client.screenCapture) {
            client.screenCapture.kill();
        }
        this.clients.delete(client.id);
    }

    generateClientId() {
        return Math.random().toString(36).substring(2, 15);
    }

    startShadowServer() {
        // Start FreeRDP shadow server
        console.log('Starting FreeRDP shadow server...');

        this.shadowProcess = spawn('freerdp-shadow', [
            '/port:3389',
            '/monitors:0',
            '/size:1920x1080'
        ]);

        this.shadowProcess.stdout.on('data', (data) => {
            console.log(`Shadow: ${data}`);
        });

        this.shadowProcess.stderr.on('data', (data) => {
            console.error(`Shadow error: ${data}`);
        });

        this.shadowProcess.on('close', (code) => {
            console.log(`Shadow server exited with code ${code}`);
            this.shadowProcess = null;
        });
    }

    stop() {
        console.log('Stopping FreeRDP Web Proxy...');

        // Close all client connections
        for (const client of this.clients.values()) {
            client.ws.close();
        }

        // Stop shadow server
        if (this.shadowProcess) {
            this.shadowProcess.kill();
        }

        // Close server
        if (this.server) {
            this.server.close();
        }
    }
}

// Command line argument parsing
function parseArgs() {
    const args = process.argv.slice(2);
    const options = {};

    for (let i = 0; i < args.length; i++) {
        const arg = args[i];
        if (arg.startsWith('--port=')) {
            options.port = parseInt(arg.split('=')[1]);
        } else if (arg.startsWith('--rdp-host=')) {
            options.rdpHost = arg.split('=')[1];
        } else if (arg.startsWith('--rdp-port=')) {
            options.rdpPort = parseInt(arg.split('=')[1]);
        } else if (arg.startsWith('--width=')) {
            options.width = parseInt(arg.split('=')[1]);
        } else if (arg.startsWith('--height=')) {
            options.height = parseInt(arg.split('=')[1]);
        } else if (arg.startsWith('--fps=')) {
            options.fps = parseInt(arg.split('=')[1]);
        }
    }

    return options;
}

// Start the server
if (require.main === module) {
    const options = parseArgs();
    const proxy = new FreeRDPWebProxy(options);

    // Handle graceful shutdown
    process.on('SIGINT', () => {
        console.log('\nReceived SIGINT, shutting down...');
        proxy.stop();
        process.exit(0);
    });

    process.on('SIGTERM', () => {
        console.log('\nReceived SIGTERM, shutting down...');
        proxy.stop();
        process.exit(0);
    });
}

module.exports = FreeRDPWebProxy;