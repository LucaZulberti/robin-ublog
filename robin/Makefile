TARGETS = robin_server robin_client
LIBS	= robin_api

ifeq ($(DEBUG), memcheck)
CFLAGS = -O0 -g
else ifeq ($(DEBUG), log)
CFLAGS = -O2 -DDEBUG=1
else ifeq ($(DEBUG), all)
CFLAGS = -O0 -g -DDEBUG=1
else
CFLAGS = -O2
endif

CFLAGS += -Wall

robin_server_SOURCES = robin_server.c robin_thread.c robin_conn.c \
					   robin_user.c robin_cip.c robin_log.c \
					   lib/password.c lib/socket.c lib/utility.c
robin_server_SYSLIBS = pthread crypt

robin_api_SOURCES = robin_api.c robin_log.c

robin_client_SOURCES = robin_client.c robin_cli.c \
					   lib/socket.c lib/utility.c
robin_client_LIBS    = robin_api

include ../make-common/common.mk
