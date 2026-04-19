# Makefile for CS3102 Practical P2: Lightweight Reliable Transport Protocol (LRTP)
#
# Saleem Bhatti <https://saleem.host.cs.st-andrews.ac.uk/>
# Jan 2024, Feb 2023, Jan 2022, Jan 2021
# checked March 2025 (sjm55)
# revised March 2026 (sjm55)

CC	=clang
#CC	=gcc
CC-flags	=-Wall -I. #-DOBFUSCATE #-DDEBUG

VPATH = tests

PROGRAMS	=   test-client-0 test-server-0 \
			    test-client-1 test-server-1 \
				test-client-2 test-server-2 \
				test-client-3 test-server-3 \
				test-adaptive-rto-client test-adaptive-rto-server \
				test-large-transfer-client test-large-transfer-server \
				test-multiple-sends-client test-multiple-sends-server \
				test-stress-client test-stress-server

H-files	= lrtp.h lrtp-common.h lrtp-pcb.h byteorder64.h d_print.h

.SUFFIXES:	.h .c .o

.c.o:;	$(CC) $(CC-flags) -c $<

LIB-files	=lrtp.o lrtp-common.o lrtp-pcb.o lrtp-fsm.o byteorder64.o d_print.o

all: $(PROGRAMS) liblrtp.a

# Build drop utility in parent directory
drop: ../drop.c
	$(CC) -o ../drop ../drop.c

# Run drop utility with arguments: make run-drop d=10 p=1000
.PHONY: run-drop
run-drop: drop
	@echo "Usage: make run-drop d=<drop%> p=<packets>"
	@if [ -z "$$d" ] || [ -z "$$p" ]; then \
		echo "Please provide d and p, e.g. make run-drop d=10 p=1000"; \
		exit 1; \
	 else \
		../drop $$d $$p; \
	 fi


liblrtp.a: $(LIB-files)
	ar ruv $@ $+

lrtp.c:	$(H-files) lrtp-packet.h lrtp-fsm.h

lrtp-common.c: lrtp-common.h

lrtp-fsm.c: lrtp-fsm.h lrtp-fsm.h

lrtp-pcb.c: lrtp-pcb.h lrtp-fsm.h

byteorder64.c:	byteorder64.h

d_print.c:	d_print.h


test-client-0: test-client-0.o liblrtp.a

tests/test-client-0.c: $(H-files)

test-server-0: test-server-0.o liblrtp.a

tests/test-server-0.c: $(H-files)


test-client-1: test-client-1.o liblrtp.a

tests/test-client-1.c: $(H-files)


test-server-1: test-server-1.o liblrtp.a

tests/test-server-1.c: $(H-files)


test-client-2: test-client-2.o liblrtp.a

tests/test-client-2.c: $(H-files)


test-server-2: test-server-2.o liblrtp.a

tests/test-server-2.c: $(H-files)


test-client-3: test-client-3.o liblrtp.a

tests/test-client-3.c: $(H-files)


test-server-3: test-server-3.o liblrtp.a

tests/test-server-3.c: $(H-files)


test-adaptive-rto-client: test-adaptive-rto-client.o liblrtp.a

tests/test-adaptive-rto-client.c: $(H-files)


test-adaptive-rto-server: test-adaptive-rto-server.o liblrtp.a

tests/test-adaptive-rto-server.c: $(H-files)


test-large-transfer-client: test-large-transfer-client.o liblrtp.a

tests/test-large-transfer-client.c: $(H-files)


test-large-transfer-server: test-large-transfer-server.o liblrtp.a

tests/test-large-transfer-server.c: $(H-files)


test-multiple-sends-client: test-multiple-sends-client.o liblrtp.a

tests/test-multiple-sends-client.c: $(H-files)


test-multiple-sends-server: test-multiple-sends-server.o liblrtp.a

tests/test-multiple-sends-server.c: $(H-files)


test-stress-client: test-stress-client.o liblrtp.a

tests/test-stress-client.c: $(H-files)


test-stress-server: test-stress-server.o liblrtp.a

tests/test-stress-server.c: $(H-files)


.PHONY: run_test_adaptive

# Use tmux to run server/client in split panes inside a single terminal.
# This works inside VS Code integrated terminal as a single split terminal pane.
# Requires `tmux` to be installed. User can override TMUX_SESSION.
TMUX := $(shell command -v tmux 2>/dev/null)
TMUX_SESSION ?= lrtp_test_$(shell basename "$(CURDIR)")

define LAUNCH_TMUX_PAIR
	if [ -z "$(TMUX)" ]; then echo "tmux not found. Install tmux or set TERMINAL to use external terminals."; exit 1; fi; \
	# Kill any existing session with same name to start fresh
	if $(TMUX) has-session -t $(TMUX_SESSION) 2>/dev/null; then $(TMUX) kill-session -t $(TMUX_SESSION); fi; \
	# Create new session with server in left pane
	$(TMUX) new-session -d -s $(TMUX_SESSION) 'cd "$(CURDIR)"; $(1)'; \
	# split horizontally and run client in right pane
	$(TMUX) split-window -h -t $(TMUX_SESSION) 'cd "$(CURDIR)"; $(2)'; \
	# select left pane and attach (user sees split panes)
	$(TMUX) select-pane -L -t $(TMUX_SESSION); \
	$(TMUX) attach-session -t $(TMUX_SESSION)
endef

.PHONY: run_test_adaptive run_test_0 run_test_1 run_test_2 run_test_3 \
        run_test_large_transfer run_test_multiple_sends run_test_stress run_all_tests

run_test_adaptive: test-adaptive-rto-server test-adaptive-rto-client
	@echo "Launching adaptive RTO server and client in tmux split panes (session=$(TMUX_SESSION))..."
	@$(call LAUNCH_TMUX_PAIR,./test-adaptive-rto-server\ 24536,./test-adaptive-rto-client\ localhost\ 24536\ 10)

run_test_0: test-server-0 test-client-0
	@echo "Launching test 0 (server/client) in tmux session=$(TMUX_SESSION)..."
	@$(call LAUNCH_TMUX_PAIR,./test-server-0,./test-client-0\ localhost)

run_test_1: test-server-1 test-client-1
	@echo "Launching test 1 (server/client) in tmux session=$(TMUX_SESSION)..."
	@$(call LAUNCH_TMUX_PAIR,./test-server-1,./test-client-1\ localhost)

run_test_2: test-server-2 test-client-2
	@echo "Launching test 2 (server/client) in tmux session=$(TMUX_SESSION)..."
	@$(call LAUNCH_TMUX_PAIR,./test-server-2,./test-client-2\ localhost)

run_test_3: test-server-3 test-client-3
	@echo "Launching test 3 (server/client) in tmux session=$(TMUX_SESSION)..."
	@$(call LAUNCH_TMUX_PAIR,./test-server-3,./test-client-3\ localhost)

run_test_large_transfer: test-large-transfer-server test-large-transfer-client
	@echo "Launching large-transfer server and client in tmux session=$(TMUX_SESSION)..."
	@$(call LAUNCH_TMUX_PAIR,./test-large-transfer-server\ 24536,./test-large-transfer-client\ localhost\ 24536)

run_test_multiple_sends: test-multiple-sends-server test-multiple-sends-client
	@echo "Launching multiple-sends server and client in tmux session=$(TMUX_SESSION)..."
	@$(call LAUNCH_TMUX_PAIR,./test-multiple-sends-server\ 24536,./test-multiple-sends-client\ localhost\ 24536)

run_test_stress: test-stress-server test-stress-client
	@echo "Launching stress server and client in tmux session=$(TMUX_SESSION)..."
	@$(call LAUNCH_TMUX_PAIR,./test-stress-server\ 24536,./test-stress-client\ localhost\ 24536)

# Run all tests sequentially (each will attach to tmux; user can detach and re-run)
run_all_tests: run_test_0 run_test_1 run_test_2 run_test_3 run_test_adaptive \
               run_test_large_transfer run_test_multiple_sends run_test_stress
	@echo "Launched all test pairs (tmux sessions)."

.PHONY: clean

clean:
	rm -rf *.o *.a $(PROGRAMS) *~ ../drop
