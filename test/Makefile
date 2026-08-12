# Builds Bulwark's real measurement and scoring engine for the host and takes
# it apart.
#
# Everything on the Flipper's screen is a rendering of what bw_stats() and
# bw_score_eval() decided, and a screenshot cannot vouch for any of it -- so
# the histogram maths, the thresholds, the family ceilings, the caps and the
# promises the README makes are checked on every push.
#
#   make -C test
#   make -C test dump     # what each demo scene actually scores

CC      ?= cc
CFLAGS  := -std=gnu2x -Wall -Wextra -Werror -O1 -g
CFLAGS  += -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS  += -I ..

ENGINE := ../helpers/bw_chan.c ../helpers/bw_stats.c ../helpers/bw_score.c
HDR    := ../helpers/bw_chan.h ../helpers/bw_stats.h ../helpers/bw_score.h ../helpers/bw_demo.h

SRC := host_score_test.c ../helpers/bw_demo.c $(ENGINE)
BIN := host_score_test

DUMP_SRC := host_demo_dump.c ../helpers/bw_demo.c $(ENGINE)
DUMP_BIN := host_demo_dump

.PHONY: all run dump clean
all: run

$(BIN): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $@

run: $(BIN)
	./$(BIN)

$(DUMP_BIN): $(DUMP_SRC) $(HDR)
	$(CC) $(CFLAGS) $(DUMP_SRC) -o $@

# Writes demo_dump.json, which tools_gen_mockups.py reads so the README
# screenshots show numbers the engine really produced.
dump: $(DUMP_BIN)
	./$(DUMP_BIN) > demo_dump.json
	@echo "wrote demo_dump.json"

clean:
	rm -rf $(BIN) $(DUMP_BIN) *.dSYM demo_dump.json
