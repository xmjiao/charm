-include ../../common.mk
-include ../../../include/conv-mach-opt.mak
CHARMC=../../../bin/charmc $(OPTS)

all: simpleGet

simpleGet: simpleGet.o
	$(CHARMC) -language converse++ -o simpleGet simpleGet.o

simpleGet.o: simpleGet.C
	$(CHARMC) -language converse++ -c simpleGet.C

test: simpleGet
	$(call run, ./simpleGet +p2 )
ifeq ($(CMK_USE_CMA),1)
	$(call run, ./simpleGet +p2 +noCMAForZC)
endif

clean:
	rm -f core *.cpm.h
	rm -f TAGS *.o
	rm -f simpleGet
	rm -f conv-host charmrun
