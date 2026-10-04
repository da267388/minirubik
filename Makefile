# minirubik IDA* (two 4-cubie PDBs, nibble packed) - host tools, gates, analysis.
CC      ?= cc
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra -Wpedantic
INC     := -Icommon -Ic -Itools -Itests
BUILD   := build
DIST    := $(BUILD)/dist.bin
D11     := $(BUILD)/d11.txt
SAMPLES := $(BUILD)/samples_d8_10.txt
CORE    := c/solver_core.c
HDRS    := c/tables.h c/pdb_spec.h c/opcount.h c/solver_core.h common/cube_model.h
H3_JOBS ?= $(shell nproc 2>/dev/null || echo 1)
RV32PFX ?= riscv64-unknown-elf-
SAMPLE_STATE := 21345671111111
COST    ?= 200
BUDGET  ?= 50000000

.PHONY: all oracle tables solver h0 h1 h2 h3 h3-quick h4 gates check vectors sample \
        stats ablate select size rv32-check clean distclean help

all: oracle tables solver

help:
	@echo "make oracle    exact BFS -> dist.bin, d11.txt, samples (host)"
	@echo "make tables    generate c/tables.h (QT + 2 nibble PDBs)"
	@echo "make solver    build the CLI solver (host harness around the core)"
	@echo "gates:  h0 (model) h1 (admissible) h2 (tables) h4 (nibble) h3 (optimal, full)"
	@echo "        h3-quick (1/367 sample)  vectors (d11 + d8-10 samples)  check (all but full h3)"
	@echo "analysis: stats (node/op counts)  ablate (switch OPT_*)  select (PDB design sweep)"
	@echo "target prep: size (static data)  rv32-check (cross-compile, no mul/div symbols)"

$(BUILD):
	mkdir -p $(BUILD)

# ---- host oracle ---------------------------------------------------------
$(BUILD)/oracle: host/oracle.c common/cube_model.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $< -o $@
$(DIST) $(D11) $(SAMPLES): $(BUILD)/oracle
	$(BUILD)/oracle $(BUILD)
oracle: $(DIST)

# ---- generated tables ------------------------------------------------------
$(BUILD)/gen_tables: tools/gen_tables.c tools/pdb_build.h c/pdb_spec.h common/cube_model.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $< -o $@
c/tables.h: $(BUILD)/gen_tables
	$(BUILD)/gen_tables $@
tables: c/tables.h

# ---- solver + gate binaries ------------------------------------------------
$(BUILD)/solver: c/main_host.c $(CORE) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) c/main_host.c $(CORE) -o $@
solver: $(BUILD)/solver

$(BUILD)/h%: tests/h%.c $(CORE) $(HDRS) tests/testutil.h tools/pdb_build.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $< $(CORE) -o $@

h0: $(BUILD)/h0_model
	$(BUILD)/h0_model
h1: $(BUILD)/h1_admissible $(DIST)
	$(BUILD)/h1_admissible $(DIST)
h2: $(BUILD)/h2_tables
	$(BUILD)/h2_tables
h4: $(BUILD)/h4_nibble
	$(BUILD)/h4_nibble
h3-quick: $(BUILD)/h3_optimal $(DIST)
	$(BUILD)/h3_optimal $(DIST) 367 0 1
h3: $(BUILD)/h3_optimal $(DIST)
	@echo "H3 over all 3,674,160 states with $(H3_JOBS) process(es)"
	@pids=""; fail=0; \
	for k in $$(seq 0 $$(($(H3_JOBS) - 1))); do \
	  $(BUILD)/h3_optimal $(DIST) 1 $$k $(H3_JOBS) & pids="$$pids $$!"; done; \
	for p in $$pids; do wait $$p || fail=1; done; \
	if [ $$fail -eq 0 ]; then echo "H3 PASS (all parts)"; else echo "H3 FAIL"; exit 1; fi

# ---- analysis ----------------------------------------------------------------
$(BUILD)/node_stats: tools/node_stats.c $(CORE) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -DOPCOUNT $(INC) tools/node_stats.c $(CORE) -o $@
vectors: $(BUILD)/node_stats $(D11) $(SAMPLES)
	$(BUILD)/node_stats $(D11) $(COST) $(BUDGET) >/dev/null
	$(BUILD)/node_stats $(SAMPLES) $(COST) $(BUDGET) >/dev/null
	@echo "vectors: every state in d11.txt and samples_d8_10.txt solved optimally"
stats: $(BUILD)/node_stats $(D11) $(SAMPLES)
	@echo "== all 2,644 distance-11 states (COST=$(COST) instr/child, BUDGET=$(BUDGET)) =="
	NODE_CSV=d11.csv \
	$(BUILD)/node_stats $(D11) $(COST) $(BUDGET)
	@echo "== 1,500 sampled states at distance 8-10 =="
	NODE_CSV=d8_10.csv \
	$(BUILD)/node_stats $(SAMPLES) $(COST) $(BUDGET)

# ablation: every OPT_* switch changes the operation counts, not the answers
ABL = baseline:-DSOLVER_OPT=0  index:-DOPT_INDEX=0  unroll:-DOPT_UNROLL=0 \
      lazy:-DOPT_LAZY=0  all-on:-DSOLVER_OPT=1
ablate: $(D11)
	@for v in $(ABL); do name=$${v%%:*}; flags=$${v#*:}; \
	  $(CC) $(CFLAGS) -DOPCOUNT $$flags $(INC) tools/node_stats.c $(CORE) -o $(BUILD)/ns_$$name || exit 1; \
	  printf '%-9s ' $$name; $(BUILD)/ns_$$name $(D11) | grep 'per state means'; done

$(BUILD)/pdb_select: tools/pdb_select.c tools/pdb_build.h tests/testutil.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $< -o $@
select: $(BUILD)/pdb_select $(DIST)
	$(BUILD)/pdb_select sweep $(DIST)

# ---- target preparation ------------------------------------------------------
size: $(BUILD)/solver
	$(CC) $(CFLAGS) $(INC) -c $(CORE) -o $(BUILD)/solver_core.o
	size -A $(BUILD)/solver_core.o
	@echo "static data = .rodata + .data + .bss must be <= 131072 bytes"
rv32-check: $(HDRS)
	@command -v $(RV32PFX)gcc >/dev/null || { echo "$(RV32PFX)gcc not found"; exit 1; }
	$(RV32PFX)gcc -O2 -march=rv32i -mabi=ilp32 -ffreestanding -fno-builtin -std=c99 -Ic -c $(CORE) -o $(BUILD)/solver_core_rv32.o
	$(RV32PFX)size -A $(BUILD)/solver_core_rv32.o
	@if $(RV32PFX)nm -u $(BUILD)/solver_core_rv32.o | grep -E '__(mul|div|udiv|mod|umod)'; then \
	  echo "FAIL: compiler helper routines referenced"; exit 1; \
	else echo "OK: no __mulsi3/__divsi3/__umodsi3 references"; fi

# ---- aggregate -----------------------------------------------------------------
sample: $(BUILD)/solver
	@n=$$($(BUILD)/solver $(SAMPLE_STATE) | wc -w); \
	test $$n -eq 11 && echo "sample $(SAMPLE_STATE): optimal 11-move solution, verified by model" \
	|| { echo "sample FAIL ($$n moves)"; exit 1; }
check: h0 h2 h4 h1 h3-quick vectors sample
	@echo "== all quick gates passed; run 'make h3' for the full optimality gate =="

clean:
	rm -rf $(BUILD)
distclean: clean
	rm -f c/tables.h
