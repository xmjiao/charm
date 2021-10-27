-include ../../../common.mk
-include ../../../../include/conv-mach-opt.mak
CHARMDIR = ../../../..
CHARMC = $(CHARMDIR)/bin/charmc $(OPTS)

all: zc_noElemOnPe

OBJS = zc_noElemOnPe.o

zc_noElemOnPe: $(OBJS)
	$(CHARMC) -language charm++ -o zc_noElemOnPe $(OBJS)

cifiles: zc_noElemOnPe.ci
	$(CHARMC)  zc_noElemOnPe.ci
	touch cifiles

zc_noElemOnPe.o: zc_noElemOnPe.C cifiles
	$(CHARMC) -c zc_noElemOnPe.C

test: all
	$(call run, +p1 ./zc_noElemOnPe)
	$(call run, +p4 ./zc_noElemOnPe)
	$(call run, +p6 ./zc_noElemOnPe)
ifeq ($(CMK_USE_CMA),1)
	$(call run, +p4 ./zc_noElemOnPe +noCMAForZC)
	$(call run, +p6 ./zc_noElemOnPe +noCMAForZC)
endif

testp: all
	$(call run, +p$(P) ./zc_noElemOnPe )
ifeq ($(CMK_USE_CMA),1)
	$(call run, +p$(P) ./zc_noElemOnPe +noCMAForZC)
endif

smptest: all
	$(call run, +p4 ./zc_noElemOnPe ++ppn 4)
	$(call run, +p6 ./zc_noElemOnPe ++ppn 3)
ifeq ($(CMK_USE_CMA),1)
	$(call run, +p4 ./zc_noElemOnPe +noCMAForZC ++ppn 4)
	$(call run, +p6 ./zc_noElemOnPe +noCMAForZC ++ppn 3)
endif

clean:
	rm -f *.decl.h *.def.h *.o
	rm -f zc_noElemOnPe charmrun cifiles
