-include ../../common.mk
-include ../../../include/conv-mach-opt.mak
CHARMC=../../../bin/ampicc

TEST_TARGETS :=

# Fails on multicore-darwin because ASLR is not disabled (issue #3555)
ifneq (1,$(CMK_MULTICORE))
  TEST_TARGETS += test-hello
else ifneq (1,$(CMK_MACOSX))
  TEST_TARGETS += test-hello
endif

all: hello

hello: hello.c
	$(CHARMC) -o $@ $< $(OPTS)

test: $(TEST_TARGETS)

test-hello:
	rm -rf log
	$(call run, +p4 ./hello +vp8 )
	-sync
	$(call run, +p4 ./hello +vp8 +restart log )
	$(call run, +p2 ./hello +vp8 +restart log )
	-sync
	rm -rf log
	$(call run, +p2 ./hello +vp8 )
	-sync
	$(call run, +p4 ./hello +vp8 +restart log )
	rm -rf log
	-sync
	$(call run, +p4 ./hello +vp8 +balancer RandCentLB )
	-sync
	$(call run, +p4 ./hello +vp8 +restart log )
	$(call run, +p2 ./hello +vp8 +restart log )
	-sync
	rm -rf log
	$(call run, +p2 ./hello +vp8 +balancer RandCentLB )
	-sync
	$(call run, +p4 ./hello +vp8 +restart log )

clean:
	rm -f *.o *.mod moduleinit.C hello *~ conv-host charmrun ampirun
	rm -rf log
