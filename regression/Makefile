TESTS=$(sort $(basename $(wildcard test*.lama)))
LOGS=$(TESTS:=.log)

LAMAC ?= $(abspath ../build/lamac)
export LAMA_RUNTIME ?= $(abspath ../runtime)

.PHONY: check $(TESTS) expr_tests clean

check: $(TESTS) expr_tests

$(TESTS): %: %.lama %.input orig/%.log
	$(LAMAC) compile $<
	./$@ < $@.input > $@.log
	diff -u orig/$@.log $@.log

expr_tests:
	$(MAKE) -C expressions
	$(MAKE) -C deep-expressions

clean:
	$(RM) $(TESTS) $(TESTS:=.s) $(LOGS)
	$(MAKE) -C expressions clean
	$(MAKE) -C deep-expressions clean
