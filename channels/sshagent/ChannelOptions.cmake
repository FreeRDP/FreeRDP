
set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT OFF)
set(OPTION_SERVER_DEFAULT OFF)

define_channel_options(NAME "sshagent" TYPE "dynamic"
	DESCRIPTION "SSH Agent Forwarding (experimental)"
	SPECIFICATIONS ""
	DEFAULT ${OPTION_DEFAULT})

define_channel_client_options(${OPTION_CLIENT_DEFAULT})

