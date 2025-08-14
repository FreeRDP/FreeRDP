This directory should contain RDP files that are checked by TestClientRdpFile

Adding new test works like this:

1. place a .rdp file in this folder
2. run TestClient TestClientRdpFile generate to generate a settings JSON from the RDP file
3. git add .rdp .json && git commit .
4. now the unit test is run and the result is compared
