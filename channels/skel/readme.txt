
This is a skeleton virtual channel plugin for freerdp
To create your own virtual channel plugin, copy this directory
then change all references of "skel" to your plugin name
remember, plugin name are 7 chars or less, no spaces or funny chars

server_chan_test.cpp is an example of how to open a channel to the client
this code needs to be compiled and run on the server in an rdp session
when connect with a client that has the "skel" plugin loaded

Jay
