-include ../../common.mk
-include ../../../include/conv-mach-opt.mak
CHARMC=../../../bin/charmc $(OPTS)

all: zcPingpong

zcPingpong: zcPingpong.o
	$(CHARMC) -language converse++ -o zcPingpong zcPingpong.o

zcPingpong.o: zcPingpong.C
	$(CHARMC) -language converse++ -c zcPingpong.C

test: zcPingpong
	$(call run, ./zcPingpong +p2 )
ifeq ($(CMK_USE_CMA),1)
	$(call run, ./zcPingpong +p2 +noCMAForZC)
endif

clean:
	rm -f core *.cpm.h
	rm -f TAGS *.o
	rm -f zcPingpong
	rm -f conv-host charmrun
