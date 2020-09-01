-include ../../../common.mk
-include ../../../../include/conv-mach-opt.mak
CHARMDIR = ../../../..
CHARMC = $(CHARMDIR)/bin/charmc $(OPTS)

all: zc_post_async_delayed zc_post_async_early

zc_post_async_delayed: zc_post_async_delayed.o
	$(CHARMC) -language charm++ -o zc_post_async_delayed zc_post_async_delayed.o

cifiles: zc_post_async.ci
	$(CHARMC)  zc_post_async.ci
	touch cifiles

zc_post_async_delayed.o: zc_post_async.C cifiles
	$(CHARMC) -DDELAYED_POST=1 -c zc_post_async.C -o zc_post_async_delayed.o

zc_post_async_early: zc_post_async_early.o
	$(CHARMC) -language charm++ -o zc_post_async_early zc_post_async_early.o

zc_post_async_early.o: zc_post_async.C cifiles
	$(CHARMC) -DDELAYED_POST=0 -c zc_post_async.C -o zc_post_async_early.o

test: all
	$(call run, +p1 ./zc_post_async_delayed)
	$(call run, +p4 ./zc_post_async_delayed)
	$(call run, +p6 ./zc_post_async_delayed)
	$(call run, +p1 ./zc_post_async_early)
	$(call run, +p4 ./zc_post_async_early)
	$(call run, +p6 ./zc_post_async_early)
ifeq ($(CMK_USE_CMA),1)
	$(call run, +p4 ./zc_post_async_delayed +noCMAForZC)
	$(call run, +p6 ./zc_post_async_delayed +noCMAForZC)
	$(call run, +p4 ./zc_post_async_early +noCMAForZC)
	$(call run, +p6 ./zc_post_async_early +noCMAForZC)
endif

testp: all
	$(call run, +p$(P) ./zc_post_async_delayed )
	$(call run, +p$(P) ./zc_post_async_early )
ifeq ($(CMK_USE_CMA),1)
	$(call run, +p$(P) ./zc_post_async_delayed +noCMAForZC)
	$(call run, +p$(P) ./zc_post_async_early +noCMAForZC)
endif

smptest: all
	$(call run, +p2 ./zc_post_async_delayed ++ppn 2)
	$(call run, +p4 ./zc_post_async_delayed ++ppn 2)
	$(call run, +p2 ./zc_post_async_early ++ppn 2)
	$(call run, +p4 ./zc_post_async_early ++ppn 2)
ifeq ($(CMK_USE_CMA),1)
	$(call run, +p2 ./zc_post_async_delayed +noCMAForZC ++ppn 2)
	$(call run, +p4 ./zc_post_async_delayed +noCMAForZC ++ppn 2)
	$(call run, +p2 ./zc_post_async_early +noCMAForZC ++ppn 2)
	$(call run, +p4 ./zc_post_async_early +noCMAForZC ++ppn 2)
endif

clean:
	rm -f *.decl.h *.def.h *.o
	rm -f zc_post_async_early zc_post_async_delayed charmrun cifiles
