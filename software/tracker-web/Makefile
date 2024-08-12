
COPT = -O2
CFLAGS += -std=gnu11 -ggdb -Wall -Wextra -Wpedantic -Wunused -Werror -pthread -D_GNU_SOURCE -fanalyzer -fPIE -fstack-protector-strong -D_FORTIFY_SOURCE=1
LDFLAGS += -lm

#CFLAGS += -D BUILD_VERSION="\"$(shell git describe --dirty --always)\""	-D BUILD_DATE="\"$(shell date '+%Y-%m-%d %H:%M:%S')\""

SRCS = 	src/main.c \
		src/tle/tle.c \
		src/adcs.c \
		src/imu.c \
		src/heading.c \
		src/gpsd.c \
		src/web/web.c \
		src/web/web_status.c \
		src/motordrive.c \
		src/util/eci.c \
		src/util/latlon.c \
		src/util/timer.c \
		src/util/timing.c

LDFLAGS += -ljson-c
LDFLAGS += -lpigpio -lgps

OBJ = ${SRCS:.c=.o}
DEP = ${SRCS:.c=.d}

LWS_DIR = ./lib/libwebsockets/
LWS_LIBSDIR = ${LWS_DIR}/build/include
LWS_OBJDIR = ${LWS_DIR}/build/lib
LDFLAGS += -L $(LWS_OBJDIR) -Wl,-Bstatic -lwebsockets -Wl,-Bdynamic

LPRED_DIR = ./lib/libpredict/
LDFLAGS += -L $(LPRED_DIR) -Wl,-Bstatic -lpredict -Wl,-Bdynamic

all: _print_banner check-gitsubmodules check-lws check-lpred motordrive

debug: COPT = -Og
debug: CFLAGS += -fno-omit-frame-pointer
debug: all

motordrive: ${OBJ}
	@echo "  LD     "$@
	@${CC} ${COPT} ${CFLAGS} -MMD -o $@ ${OBJ} ${LDFLAGS}

%.o: %.c
	@echo "  CC     "$<
	@${CC} ${COPT} ${CFLAGS} -I src/ -I $(LWS_LIBSDIR) -I $(LPRED_DIR) -MMD -c -fPIC -o $@ $<

_print_banner:
	@echo "Compiling with GCC $(shell $(CC) -dumpfullversion) on $(shell $(CC) -dumpmachine)"

clean:
	@rm -rf motordrive ${OBJ} ${DEP}

check-gitsubmodules:
	@if git submodule status | egrep -q '^[-]|^[+]' ; then \
		echo "INFO: Need to [re]initialize git submodules"; \
		git submodule update --init; \
	fi

check-lws:
	@if [ ! -f "${LWS_OBJDIR}/libwebsockets.a" ]; then \
		echo "INFO: Need to compile libwebsockets"; \
		mkdir -p ${LWS_DIR}/build/; \
		cd ${LWS_DIR}/build/; \
		cmake ../ -DLWS_WITH_SSL=off \
			-DLWS_WITH_SHARED=off \
			-DLWS_WITHOUT_CLIENT=on \
			-DLWS_WITHOUT_TESTAPPS=on; \
		make -j4; \
	fi

check-lpred:
	@if [ ! -f "${LPRED_DIR}/libpredict.a" ]; then \
		echo "INFO: Need to compile libpredict"; \
		cd ${LPRED_DIR}/; \
		make static -j4; \
	fi

.PHONY: all _print_banner clean
