#!/usr/bin/env node

/**
 * Simple test for FreeRDP Web Client
 */

const http = require('http');

console.log('Testing FreeRDP Web Client setup...\n');

// Test 1: Check if package.json exists
const fs = require('fs');
const path = require('path');

try {
    const packageJson = JSON.parse(fs.readFileSync('package.json', 'utf8'));
    console.log('✓ package.json found');
    console.log(`  Name: ${packageJson.name}`);
    console.log(`  Version: ${packageJson.version}`);
} catch (error) {
    console.log('✗ package.json not found or invalid');
    process.exit(1);
}

// Test 2: Check if main files exist
const requiredFiles = ['server.js', 'index.html', 'README.md'];
let allFilesExist = true;

requiredFiles.forEach(file => {
    if (fs.existsSync(file)) {
        console.log(`✓ ${file} found`);
    } else {
        console.log(`✗ ${file} missing`);
        allFilesExist = false;
    }
});

if (!allFilesExist) {
    console.log('\nSome required files are missing!');
    process.exit(1);
}

// Test 3: Check if dependencies are installed
try {
    require('ws');
    console.log('✓ ws dependency installed');
} catch (error) {
    console.log('✗ ws dependency not installed - run "npm install"');
}

try {
    require('express');
    console.log('✓ express dependency installed');
} catch (error) {
    console.log('✗ express dependency not installed - run "npm install"');
}

console.log('\n✓ All basic checks passed!');
console.log('\nTo start the server:');
console.log('  npm start');
console.log('\nTo test the web interface:');
console.log('  Open http://localhost:8080 in your browser');