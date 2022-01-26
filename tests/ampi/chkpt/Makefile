-include ../../common.mk
-include ../../../include/conv-mach-opt.mak
AMPICC=../../../bin/ampicc
AMPIF90=../../../bin/ampif90

TARGETS := hello
TEST_CANDIDATES := test-hello

ifeq (1,$(CMK_CAN_LINK_FORTRAN))
  TARGETS += fhello
  TEST_CANDIDATES += test-fhello
endif

TEST_TARGETS :=
# Fails on multicore-darwin because ASLR is not disabled (issue #3555)
ifneq (1,$(CMK_MULTICORE))
  TEST_TARGETS += $(TEST_CANDIDATES)
else ifneq (1,$(CMK_MACOSX))
  TEST_TARGETS += $(TEST_CANDIDATES)
endif

all: $(TARGETS)

hello: hello.c
	$(AMPICC) -o $@ $< $(OPTS)

fhello: fhello.f90
	$(AMPIF90) -o $@ $< $(OPTS)

test: $(TEST_TARGETS)

test-hello: hello
	rm -rf log
	$(call run, +p4 ./$< +vp8 )
	-sync
	$(call run, +p4 ./$< +vp8 +restart log )
	$(call run, +p2 ./$< +vp8 +restart log )
	-sync
	rm -rf log
	$(call run, +p2 ./$< +vp8 )
	-sync
	$(call run, +p4 ./$< +vp8 +restart log )
	rm -rf log
	-sync
	$(call run, +p4 ./$< +vp8 +balancer RandCentLB )
	-sync
	$(call run, +p4 ./$< +vp8 +restart log )
	$(call run, +p2 ./$< +vp8 +restart log )
	-sync
	rm -rf log
	$(call run, +p2 ./$< +vp8 +balancer RandCentLB )
	-sync
	$(call run, +p4 ./$< +vp8 +restart log )

test-fhello: fhello
	rm -rf log
	$(call run, +p4 ./$< +vp8 )
	-sync
	$(call run, +p4 ./$< +vp8 +restart log )
	$(call run, +p2 ./$< +vp8 +restart log )
	-sync
	rm -rf log
	$(call run, +p2 ./$< +vp8 )
	-sync
	$(call run, +p4 ./$< +vp8 +restart log )
	rm -rf log
	-sync
	$(call run, +p4 ./$< +vp8 +balancer RandCentLB )
	-sync
	$(call run, +p4 ./$< +vp8 +restart log )
	$(call run, +p2 ./$< +vp8 +restart log )
	-sync
	rm -rf log
	$(call run, +p2 ./$< +vp8 +balancer RandCentLB )
	-sync
	$(call run, +p4 ./$< +vp8 +restart log )

clean:
	rm -f *.o *.mod moduleinit.C hello fhello *~ conv-host charmrun ampirun
	rm -rf log
