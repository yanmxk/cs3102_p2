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

.PHONY: clean

clean:
	rm -rf *.o *.a $(PROGRAMS) *~ ../drop
