TARGETS = robin_server
#TARGETS = robin_server robin_client
#LIBS	= robin_api

robin_server_SOURCES = robin_server.c robin_thread.c robin_manager.c \
					   robin_log.c
robin_server_SYSLIBS = pthread

robin_api_SOURCES = robin_api.c

robin_client_SOURCES = robin_client.c
robin_client_LIBS    = robin_api

include ../make-common/common.mk
