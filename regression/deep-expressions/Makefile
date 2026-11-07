TESTS=$(sort $(basename $(wildcard generated*.lama)))
LOGS=$(TESTS:=.log)

LAMAC ?= $(abspath ../../build/lamac)
export LAMA_RUNTIME ?= $(abspath ../../runtime)

.PHONY: check $(TESTS) clean

check: $(TESTS)

$(TESTS): %: %.lama %.input orig/%.log
	$(LAMAC) compile $<
	./$@ < $@.input > $@.log
	diff -u orig/$@.log $@.log

clean:
	$(RM) $(TESTS) $(TESTS:=.s) $(LOGS)
