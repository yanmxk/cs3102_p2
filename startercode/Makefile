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

# Detect a terminal emulator on Linux. User can override by setting TERMINAL env var.
TERMINAL := $(shell for t in gnome-terminal xterm konsole lxterminal mate-terminal; do command -v $$t >/dev/null 2>&1 && { echo $$t; exit 0; }; done)

# Default server hostname (used as suggestion when prompting for server address)
SERVER_DEFAULT := $(shell hostname -f 2>/dev/null || hostname)

run_test_adaptive: test-adaptive-rto-server test-adaptive-rto-client
	@echo "Launching adaptive RTO server and client in new terminal windows..."
	@TER="$(TERMINAL)"; \
	if [ -z "$$TER" ]; then echo "No terminal emulator found. Set TERMINAL env var."; exit 1; fi; \
	HOST="$${MAKE_HOST:-}"; \
	if [ -z "$$HOST" ]; then printf "Enter remote client host (user@host): "; read -r HOST; fi; \
	SERVER_ADDR="$${MAKE_SERVER:-$(SERVER_DEFAULT)}"; \
	if [ -z "$$SERVER_ADDR" ]; then printf "Enter server address reachable from remote client (e.g. your.IP.or.host): "; read -r SERVER_ADDR; fi; \
	CMD='cd "$(CURDIR)"; ./test-adaptive-rto-server 24536; echo "Server exited"; read -n 1 -s -r -p "Press any key to close"'; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD"; \
	else $$TER -e bash -lc "$$CMD"; fi; \
	sleep 1; \
	CMD2="ssh -t $$HOST 'cd ~/cs3102_p2/startercode && ./test-adaptive-rto-client $$SERVER_ADDR 24536 10; echo "Client exited"; read -n 1 -s -r -p "Press any key to close"'"; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD2"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD2"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD2"; \
	else $$TER -e bash -lc "$$CMD2"; fi

# Convenience targets to launch each test pair in new terminal windows (Linux)
.PHONY: run_test_0 run_test_1 run_test_2 run_test_3 \
        run_test_large_transfer run_test_multiple_sends run_test_stress run_all_tests

run_test_0: test-server-0 test-client-0
	@echo "Launching test-client-0/test-server-0..."
	@TER="$(TERMINAL)"; \
	if [ -z "$$TER" ]; then echo "No terminal emulator found. Set TERMINAL env var."; exit 1; fi; \
	HOST="$${MAKE_HOST:-}"; \
	if [ -z "$$HOST" ]; then printf "Enter remote client host (user@host): "; read -r HOST; fi; \
	SERVER_ADDR="$${MAKE_SERVER:-$(SERVER_DEFAULT)}"; \
	if [ -z "$$SERVER_ADDR" ]; then printf "Enter server address reachable from remote client (e.g. your.IP.or.host): "; read -r SERVER_ADDR; fi; \
	CMD='cd "$(CURDIR)"; ./test-server-0; echo "Server exited"; read -n 1 -s -r -p "Press any key to close"'; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD"; \
	else $$TER -e bash -lc "$$CMD"; fi; \
	sleep 1; \
	CMD2="ssh -t $$HOST 'cd ~/cs3102_p2/startercode && ./test-client-0 $$SERVER_ADDR; echo \"Client exited\"; read -n 1 -s -r -p \"Press any key to close\"'"; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD2"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD2"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD2"; \
	else $$TER -e bash -lc "$$CMD2"; fi

run_test_1: test-server-1 test-client-1
	@echo "Launching test-client-1/test-server-1..."
	@TER="$(TERMINAL)"; \
	if [ -z "$$TER" ]; then echo "No terminal emulator found. Set TERMINAL env var."; exit 1; fi; \
	HOST="$${MAKE_HOST:-}"; \
	if [ -z "$$HOST" ]; then printf "Enter remote client host (user@host): "; read -r HOST; fi; \
	SERVER_ADDR="$${MAKE_SERVER:-$(SERVER_DEFAULT)}"; \
	if [ -z "$$SERVER_ADDR" ]; then printf "Enter server address reachable from remote client (e.g. your.IP.or.host): "; read -r SERVER_ADDR; fi; \
	CMD='cd "$(CURDIR)"; ./test-server-1; echo "Server exited"; read -n 1 -s -r -p "Press any key to close"'; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD"; \
	else $$TER -e bash -lc "$$CMD"; fi; \
	sleep 1; \
	CMD2="ssh -t $$HOST 'cd ~/cs3102_p2/startercode && ./test-client-1 $$SERVER_ADDR; echo \"Client exited\"; read -n 1 -s -r -p \"Press any key to close\"'"; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD2"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD2"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD2"; \
	else $$TER -e bash -lc "$$CMD2"; fi

run_test_2: test-server-2 test-client-2
	@echo "Launching test-client-2/test-server-2..."
	@TER="$(TERMINAL)"; \
	if [ -z "$$TER" ]; then echo "No terminal emulator found. Set TERMINAL env var."; exit 1; fi; \
	HOST="$${MAKE_HOST:-}"; \
	if [ -z "$$HOST" ]; then printf "Enter remote client host (user@host): "; read -r HOST; fi; \
	SERVER_ADDR="$${MAKE_SERVER:-$(SERVER_DEFAULT)}"; \
	if [ -z "$$SERVER_ADDR" ]; then printf "Enter server address reachable from remote client (e.g. your.IP.or.host): "; read -r SERVER_ADDR; fi; \
	CMD='cd "$(CURDIR)"; ./test-server-2; echo "Server exited"; read -n 1 -s -r -p "Press any key to close"'; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD"; \
	else $$TER -e bash -lc "$$CMD"; fi; \
	sleep 1; \
	CMD2="ssh -t $$HOST 'cd ~/cs3102_p2/startercode && ./test-client-2 $$SERVER_ADDR; echo \"Client exited\"; read -n 1 -s -r -p \"Press any key to close\"'"; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD2"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD2"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD2"; \
	else $$TER -e bash -lc "$$CMD2"; fi

run_test_3: test-server-3 test-client-3
	@echo "Launching test-client-3/test-server-3..."
	@TER="$(TERMINAL)"; \
	if [ -z "$$TER" ]; then echo "No terminal emulator found. Set TERMINAL env var."; exit 1; fi; \
	HOST="$${MAKE_HOST:-}"; \
	if [ -z "$$HOST" ]; then printf "Enter remote client host (user@host): "; read -r HOST; fi; \
	SERVER_ADDR="$${MAKE_SERVER:-$(SERVER_DEFAULT)}"; \
	if [ -z "$$SERVER_ADDR" ]; then printf "Enter server address reachable from remote client (e.g. your.IP.or.host): "; read -r SERVER_ADDR; fi; \
	CMD='cd "$(CURDIR)"; ./test-server-3; echo "Server exited"; read -n 1 -s -r -p "Press any key to close"'; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD"; \
	else $$TER -e bash -lc "$$CMD"; fi; \
	sleep 1; \
	CMD2="ssh -t $$HOST 'cd ~/cs3102_p2/startercode && ./test-client-3 $$SERVER_ADDR; echo \"Client exited\"; read -n 1 -s -r -p \"Press any key to close\"'"; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD2"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD2"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD2"; \
	else $$TER -e bash -lc "$$CMD2"; fi

run_test_large_transfer: test-large-transfer-server test-large-transfer-client
	@echo "Launching large-transfer server and client..."
	@TER="$(TERMINAL)"; \
	if [ -z "$$TER" ]; then echo "No terminal emulator found. Set TERMINAL env var."; exit 1; fi; \
	HOST="$${MAKE_HOST:-}"; \
	if [ -z "$$HOST" ]; then printf "Enter remote client host (user@host): "; read -r HOST; fi; \
	SERVER_ADDR="$${MAKE_SERVER:-$(SERVER_DEFAULT)}"; \
	if [ -z "$$SERVER_ADDR" ]; then printf "Enter server address reachable from remote client (e.g. your.IP.or.host): "; read -r SERVER_ADDR; fi; \
	CMD='cd "$(CURDIR)"; ./test-large-transfer-server 24536; echo "Server exited"; read -n 1 -s -r -p "Press any key to close"'; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD"; \
	else $$TER -e bash -lc "$$CMD"; fi; \
	sleep 1; \
	CMD2="ssh -t $$HOST 'cd ~/cs3102_p2/startercode && ./test-large-transfer-client $$SERVER_ADDR 24536; echo \"Client exited\"; read -n 1 -s -r -p \"Press any key to close\"'"; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD2"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD2"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD2"; \
	else $$TER -e bash -lc "$$CMD2"; fi

run_test_multiple_sends: test-multiple-sends-server test-multiple-sends-client
	@echo "Launching multiple-sends server and client..."
	@TER="$(TERMINAL)"; \
	if [ -z "$$TER" ]; then echo "No terminal emulator found. Set TERMINAL env var."; exit 1; fi; \
	HOST="$${MAKE_HOST:-}"; \
	if [ -z "$$HOST" ]; then printf "Enter remote client host (user@host): "; read -r HOST; fi; \
	SERVER_ADDR="$${MAKE_SERVER:-$(SERVER_DEFAULT)}"; \
	if [ -z "$$SERVER_ADDR" ]; then printf "Enter server address reachable from remote client (e.g. your.IP.or.host): "; read -r SERVER_ADDR; fi; \
	CMD='cd "$(CURDIR)"; ./test-multiple-sends-server 24536; echo "Server exited"; read -n 1 -s -r -p "Press any key to close"'; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD"; \
	else $$TER -e bash -lc "$$CMD"; fi; \
	sleep 1; \
	CMD2="ssh -t $$HOST 'cd ~/cs3102_p2/startercode && ./test-multiple-sends-client $$SERVER_ADDR 24536; echo \"Client exited\"; read -n 1 -s -r -p \"Press any key to close\"'"; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD2"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD2"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD2"; \
	else $$TER -e bash -lc "$$CMD2"; fi

run_test_stress: test-stress-server test-stress-client
	@echo "Launching stress server and client..."
	@TER="$(TERMINAL)"; \
	if [ -z "$$TER" ]; then echo "No terminal emulator found. Set TERMINAL env var."; exit 1; fi; \
	HOST="$${MAKE_HOST:-}"; \
	if [ -z "$$HOST" ]; then printf "Enter remote client host (user@host): "; read -r HOST; fi; \
	SERVER_ADDR="$${MAKE_SERVER:-$(SERVER_DEFAULT)}"; \
	if [ -z "$$SERVER_ADDR" ]; then printf "Enter server address reachable from remote client (e.g. your.IP.or.host): "; read -r SERVER_ADDR; fi; \
	CMD='cd "$(CURDIR)"; ./test-stress-server 24536; echo "Server exited"; read -n 1 -s -r -p "Press any key to close"'; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD"; \
	else $$TER -e bash -lc "$$CMD"; fi; \
	sleep 1; \
	CMD2="ssh -t $$HOST 'cd ~/cs3102_p2/startercode && ./test-stress-client $$SERVER_ADDR 24536; echo \"Client exited\"; read -n 1 -s -r -p \"Press any key to close\"'"; \
	if [ "$$TER" = "gnome-terminal" ]; then $$TER -- bash -lc "$$CMD2"; \
	elif [ "$$TER" = "xterm" ]; then $$TER -hold -e bash -lc "$$CMD2"; \
	elif [ "$$TER" = "konsole" ]; then $$TER -e bash -lc "$$CMD2"; \
	else $$TER -e bash -lc "$$CMD2"; fi

# Run all tests (will open many terminal windows)
run_all_tests: run_test_0 run_test_1 run_test_2 run_test_3 run_test_adaptive \
               run_test_large_transfer run_test_multiple_sends run_test_stress
	@echo "Launched all test pairs (check terminal windows)."

.PHONY: clean

clean:
	rm -rf *.o *.a $(PROGRAMS) *~ ../drop
