
COPT = -O2
CFLAGS += -std=gnu11 -ggdb -Wall -Wextra -Wpedantic -Wunused -Werror -pthread -D_GNU_SOURCE -fanalyzer -fPIE -fstack-protector-all -D_FORTIFY_SOURCE=2
LDFLAGS += -lm

#CFLAGS += -D BUILD_VERSION="\"$(shell git describe --dirty --always)\""	-D BUILD_DATE="\"$(shell date '+%Y-%m-%d %H:%M:%S')\""

SRCS = 	src/main.c \
		src/adc_encoder.c \
		src/imu.c \
		src/gpsd.c \
		src/motordrive.c \
		src/util/eci.c \
		src/util/latlon.c \
		src/util/timer.c \
		src/util/timing.c

#LDFLAGS += -ljson-c
LDFLAGS += -lpigpio -lgps

OBJ = ${SRCS:.c=.o}
DEP = ${SRCS:.c=.d}

all: _print_banner motordrive

debug: COPT = -Og
debug: CFLAGS += -fno-omit-frame-pointer
debug: all

motordrive: ${OBJ}
	@echo "  LD     "$@
	@${CC} ${COPT} ${CFLAGS} -MMD -MP -o $@ ${OBJ} ${LDFLAGS}

%.o: %.c
	@echo "  CC     "$<
	@${CC} ${COPT} ${CFLAGS} -MMD -MP -c -I src/ -fPIC -o $@ $<

_print_banner:
	@echo "Compiling with GCC $(shell $(CC) -dumpfullversion) on $(shell $(CC) -dumpmachine)"

clean:
	@rm -rf motordrive ${OBJ} ${DEP}

.PHONY: all _print_banner clean
