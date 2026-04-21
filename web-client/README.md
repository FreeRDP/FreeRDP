# FreeRDP Web Remote Desktop

A web-based remote desktop client for FreeRDP that enables streaming Windows desktops to web browsers with low latency and high performance (60fps @ 1080p).

**🌐 Live Demo**: [https://freerdp.github.io/FreeRDP/](https://freerdp.github.io/FreeRDP/)

## Features

- **High Performance**: 60fps streaming at 1080p resolution
- **Low Latency**: WebRTC-based streaming for minimal delay
- **Cross-Platform**: Works on any modern web browser
- **Secure**: WebSocket-based communication with optional TLS
- **Easy Deployment**: Static files hosted on GitHub Pages

## Architecture

```
┌─────────────────┐    WebSocket    ┌─────────────────┐    RDP    ┌─────────────────┐
│   Web Browser   │◄──────────────►│  Web Proxy      │◄────────►│ FreeRDP Shadow  │
│                 │                 │  Server         │           │ Server          │
│  index.html     │                 │  (Node.js)      │           │ (Windows PC)    │
└─────────────────┘                 └─────────────────┘           └─────────────────┘
```

## Setup Instructions

### 1. Windows PC Setup (Host)

1. **Install Node.js** (version 16 or higher):
   ```bash
   # Download from https://nodejs.org/
   ```

2. **Clone and setup FreeRDP**:
   ```bash
   git clone https://github.com/FreeRDP/FreeRDP.git
   cd FreeRDP/web-client
   npm install
   ```

3. **Start the web proxy server**:
   ```bash
   # Basic setup
   npm start

   # Custom configuration
   node server.js --port=8080 --width=1920 --height=1080 --fps=60
   ```

4. **Start FreeRDP Shadow Server** (in another terminal):
   ```bash
   freerdp-shadow /port:3389 /monitors:0 /size:1920x1080
   ```

### 2. **Access the Web Client**
   - Visit the official FreeRDP web client: [https://freerdp.github.io/FreeRDP/](https://freerdp.github.io/FreeRDP/)
   - Enter your Windows PC's address (e.g., `your-pc-ip:8080`)
   - Click "Connect"

*Note: The web client is automatically deployed to GitHub Pages when changes are pushed to the main FreeRDP repository.*

## Configuration Options

### Web Proxy Server

| Option | Default | Description |
|--------|---------|-------------|
| `--port` | 8080 | WebSocket server port |
| `--rdp-host` | localhost | RDP server hostname |
| `--rdp-port` | 3389 | RDP server port |
| `--width` | 1920 | Screen width |
| `--height` | 1080 | Screen height |
| `--fps` | 60 | Target frame rate |

### FreeRDP Shadow Server

| Option | Description |
|--------|-------------|
| `/port:3389` | RDP listening port |
| `/monitors:0` | Monitor to capture (0 = primary) |
| `/size:1920x1080` | Capture resolution |

## Network Setup

For the web client to connect to your Windows PC:

1. **Local Network**: If both devices are on the same network, use the PC's local IP
2. **Internet Access**: For remote access, you'll need:
   - Public IP address or dynamic DNS
   - Port forwarding on your router (port 8080)
   - Consider using a VPN or secure tunnel

## Security Considerations

- The current implementation uses plain WebSockets (ws://)
- For production use, implement TLS (wss://)
- Consider authentication mechanisms
- Use firewalls to restrict access

## Performance Optimization

- **Hardware Acceleration**: Ensure your GPU drivers are up to date
- **Network**: Use wired connection for best performance
- **Compression**: The server uses JPEG compression (configurable)
- **Resolution**: Lower resolution for slower networks

## Troubleshooting

### Connection Issues
- Check that the web proxy server is running
- Verify firewall settings allow port 8080
- Ensure FreeRDP shadow server is started

### Performance Issues
- Reduce resolution or frame rate
- Check network latency with `ping`
- Monitor system resources (CPU, RAM, GPU)

### Browser Compatibility
- Chrome/Edge: Best performance
- Firefox: Good performance
- Safari: Limited WebRTC support

## Development

### Building from Source

```bash
# Install dependencies
npm install

# Start development server
npm run dev

# Build for production
npm run build
```

### API Reference

#### WebSocket Messages

**Client → Server:**
```json
{
  "type": "connect",
  "auth": { "username": "user", "password": "pass" }
}
```

**Server → Client:**
```json
{
  "type": "connected",
  "width": 1920,
  "height": 1080
}
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## License

Apache License 2.0 - see LICENSE file for details