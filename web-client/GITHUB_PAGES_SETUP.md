# GitHub Pages Setup for FreeRDP Web Client

## Enable GitHub Pages

To enable the web remote desktop client on GitHub Pages for this repository:

1. **Go to Repository Settings**:
   - Visit https://github.com/FreeRDP/FreeRDP/settings
   - Scroll down to "Pages" section

2. **Configure Source**:
   - Select "Deploy from a branch"
   - Choose `master` branch
   - Set folder to `/web-client`
   - Click "Save"

3. **Wait for Deployment**:
   - The GitHub Actions workflow will automatically deploy the web client
   - Visit https://freerdp.github.io/FreeRDP/ to access the remote desktop client

## Manual Deployment

If you need to trigger deployment manually:

1. Go to the "Actions" tab in the repository
2. Find the "Deploy Web Client to GitHub Pages" workflow
3. Click "Run workflow"

## Testing

After enabling GitHub Pages, the web client will be available at:
https://freerdp.github.io/FreeRDP/

You can test the connection by:
1. Running the web proxy server on your Windows PC: `npm start`
2. Starting FreeRDP shadow server: `freerdp-shadow /port:3389 /monitors:0 /size:1920x1080`
3. Connecting via the web interface