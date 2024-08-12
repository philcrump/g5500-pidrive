
COPT = -O3
CFLAGS += -std=gnu11 -ggdb -Wall -Wextra -Wpedantic -Wunused -Werror -pthread -D_GNU_SOURCE
LDFLAGS += -lm

LDFLAGS += -lpigpio

all: _print_banner clean motordrive adc_encoder adc_supplies relays i2c_imu az_motordrive el_motordrive

debug: COPT = -Og
debug: CFLAGS += -fno-omit-frame-pointer
debug: all

motordrive:
	@echo "  CC     "$@
	@${CC} ${COPT} ${CFLAGS} -o $@ $@.c ${LDFLAGS}

az_motordrive:
	@echo "  CC     "$@
	@${CC} ${COPT} ${CFLAGS} -o $@ $@.c ${LDFLAGS}

el_motordrive:
	@echo "  CC     "$@
	@${CC} ${COPT} ${CFLAGS} -o $@ $@.c ${LDFLAGS}

adc_encoder:
	@echo "  CC     "$@
	@${CC} ${COPT} ${CFLAGS} -o $@ $@.c ${LDFLAGS}

adc_supplies:
	@echo "  CC     "$@
	@${CC} ${COPT} ${CFLAGS} -o $@ $@.c ${LDFLAGS}

relays:
	@echo "  CC     "$@
	@${CC} ${COPT} ${CFLAGS} -o $@ $@.c ${LDFLAGS}

i2c_imu:
	@echo "  CC     "$@
	@${CC} ${COPT} ${CFLAGS} -o $@ $@.c ${LDFLAGS}

_print_banner:
	@echo "Compiling with GCC $(shell $(CC) -dumpfullversion) on $(shell $(CC) -dumpmachine)"

clean:
	@rm -rf motordrive adc_encoder adc_supplies relays i2c_imu az_motordrive el_motordrive

.PHONY: all _print_banner clean
