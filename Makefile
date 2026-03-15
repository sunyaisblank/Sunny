# =============================================================================
# Sunny — Build, Test, and Analysis Harness
# =============================================================================
#
# All generated output goes to dotfile directories:
#   .bin/            — CMake build tree (Release)
#   .mull-build/     — Mull mutation testing build (clang-18, libc++)
#   .codeql-db/      — CodeQL database
#   .codeql-results/ — CodeQL SARIF output
#   .coverage-cpp/   — llvm-cov coverage reports
#   .mull-report/    — Mull HTML/JSON reports
#
# Quick reference:
#   make              — build + test (default)
#   make build        — configure + compile (Release)
#   make test         — run 1560 Catch2 tests
#   make analysis     — codeql + mull (full analysis suite)
#   make codeql       — static analysis with custom queries
#   make mull         — mutation testing
#   make coverage     — line/branch coverage via llvm-cov
#   make clean        — remove all generated directories
#   make help         — show all targets
#
# =============================================================================

.PHONY: all build configure compile test \
        analysis codeql mull coverage \
        python-test python-lint python-check \
        clean clean-build clean-analysis clean-mull clean-codeql clean-coverage \
        help info

# -----------------------------------------------------------------------------
# Configuration
# -----------------------------------------------------------------------------

# Compilers
CXX           ?= g++
MULL_CXX      := clang++-18
MULL_CC       := clang-18

# Tool paths
CODEQL        := codeql
MULL_RUNNER   := mull-runner-18
LLVM_COV      := llvm-cov-18
LLVM_PROFDATA := llvm-profdata-18

# Directories
BUILD_DIR     := .bin
MULL_DIR      := .mull-build
CODEQL_DB     := .codeql-db
CODEQL_OUT    := .codeql-results
COVERAGE_DIR  := .coverage-cpp
MULL_REPORT   := .mull-report
ANALYSIS_DIR  := .analysis

# Build settings
CMAKE_FLAGS   := -DCMAKE_BUILD_TYPE=Release
MULL_CMAKE    := -DCMAKE_BUILD_TYPE=Debug \
                 -DCMAKE_C_COMPILER=$(MULL_CC) \
                 -DCMAKE_CXX_COMPILER=$(MULL_CXX) \
                 -DCMAKE_CXX_FLAGS="-stdlib=libc++" \
                 -DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++ -lc++abi" \
                 -DSUNNY_MULL_INSTRUMENT=ON \
                 -DSUNNY_BUILD_PYTHON_BINDINGS=OFF \
                 -DSUNNY_BUILD_MCP_SERVER=OFF
COV_CMAKE     := -DCMAKE_BUILD_TYPE=Debug \
                 -DCMAKE_C_COMPILER=$(MULL_CC) \
                 -DCMAKE_CXX_COMPILER=$(MULL_CXX) \
                 -DCMAKE_CXX_FLAGS="-stdlib=libc++ -fprofile-instr-generate -fcoverage-mapping" \
                 -DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++ -lc++abi -fprofile-instr-generate" \
                 -DSUNNY_BUILD_PYTHON_BINDINGS=OFF \
                 -DSUNNY_BUILD_MCP_SERVER=OFF

# Parallelism
JOBS          ?= $(shell nproc 2>/dev/null || echo 4)

# Test targets (all four Catch2 executables)
TEST_BINS     := Sunny.Test.Core Sunny.Test.Render Sunny.Test.Infrastructure Sunny.Test.Max

# Subdirectory where CMake places test executables (relative to build root)
TEST_SUBDIR   := src/Sunny.Test

# =============================================================================
# Default target
# =============================================================================

all: build test

# =============================================================================
# Build (Release)
# =============================================================================

configure: $(BUILD_DIR)/CMakeCache.txt

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	cmake -B $(BUILD_DIR) $(CMAKE_FLAGS)

compile: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

build: compile

# =============================================================================
# Test
# =============================================================================

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure -j$(JOBS)

# Verbose: show each test name
test-verbose: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure --verbose

# =============================================================================
# CodeQL Static Analysis
# =============================================================================
#
# Creates a CodeQL database from the Release build, then runs the three
# custom queries in .analysis/codeql/ plus the standard cpp-security suite.
#
# Output: .codeql-results/*.sarif (machine-readable)
#         .codeql-results/*.csv   (human-readable summary)
# =============================================================================

codeql: codeql-create codeql-analyze

codeql-create:
	@echo "=== CodeQL: creating database ==="
	rm -rf $(CODEQL_DB) .codeql-build
	$(CODEQL) database create $(CODEQL_DB) \
		--language=cpp \
		--command="$(ANALYSIS_DIR)/codeql-build.sh $(JOBS)" \
		--overwrite
	rm -rf .codeql-build

codeql-analyze: | $(CODEQL_DB)
	@echo "=== CodeQL: running analysis ==="
	mkdir -p $(CODEQL_OUT)
	$(CODEQL) database analyze $(CODEQL_DB) \
		$(ANALYSIS_DIR)/codeql/ \
		--format=sarifv2.1.0 \
		--output=$(CODEQL_OUT)/sunny-custom.sarif \
		--threads=$(JOBS)
	@echo "=== CodeQL: results in $(CODEQL_OUT)/sunny-custom.sarif ==="
	@$(CODEQL) database interpret-results $(CODEQL_DB) \
		--format=csv \
		--output=$(CODEQL_OUT)/sunny-custom.csv \
		$(ANALYSIS_DIR)/codeql/ 2>/dev/null || true

$(CODEQL_DB):
	@echo "Error: CodeQL database not found. Run 'make codeql-create' first."
	@exit 1

# =============================================================================
# Mull Mutation Testing
# =============================================================================
#
# Builds with clang-18 + libc++ + Mull IR frontend pass, then runs
# mull-runner-18 against each test executable. Each run produces a
# report in .mull-report/.
#
# Prerequisites: clang-18, libc++-18-dev, mull-18
# =============================================================================

mull: mull-build mull-run

mull-build:
	@echo "=== Mull: configuring with clang-18 + Mull instrumentation ==="
	cmake -B $(MULL_DIR) $(MULL_CMAKE)
	@echo "=== Mull: building ==="
	cmake --build $(MULL_DIR) -j$(JOBS)

mull-run: | $(MULL_DIR)
	@echo "=== Mull: running mutation tests ==="
	mkdir -p $(MULL_REPORT)
	@for test_bin in $(TEST_BINS); do \
		bin_path="$(MULL_DIR)/$(TEST_SUBDIR)/$$test_bin"; \
		if [ -f "$$bin_path" ]; then \
			echo "--- Mull: $$test_bin ---"; \
			$(MULL_RUNNER) \
				--reporters=Elements \
				--report-dir=$(MULL_REPORT) \
				--report-name=$$test_bin \
				$$bin_path \
				-- --order rand 2>&1 | tail -5; \
			echo ""; \
		fi; \
	done
	@echo "=== Mull: reports in $(MULL_REPORT)/ ==="

$(MULL_DIR):
	@echo "Error: Mull build directory not found. Run 'make mull-build' first."
	@exit 1

# =============================================================================
# Code Coverage (llvm-cov)
# =============================================================================
#
# Builds with clang-18 instrumentation (-fprofile-instr-generate
# -fcoverage-mapping), runs tests to generate .profraw files, merges
# them, and produces HTML + summary reports.
#
# Prerequisites: clang-18, llvm-cov-18, llvm-profdata-18
# =============================================================================

coverage: coverage-build coverage-run coverage-report

coverage-build:
	@echo "=== Coverage: configuring with clang-18 instrumentation ==="
	cmake -B $(COVERAGE_DIR) $(COV_CMAKE)
	@echo "=== Coverage: building ==="
	cmake --build $(COVERAGE_DIR) -j$(JOBS)

coverage-run: | $(COVERAGE_DIR)
	@echo "=== Coverage: running tests ==="
	@for test_bin in $(TEST_BINS); do \
		bin_path="$(COVERAGE_DIR)/$(TEST_SUBDIR)/$$test_bin"; \
		if [ -f "$$bin_path" ]; then \
			echo "  Running $$test_bin"; \
			LLVM_PROFILE_FILE="$(COVERAGE_DIR)/$$test_bin.profraw" $$bin_path --order rand 2>/dev/null; \
		fi; \
	done
	@echo "=== Coverage: merging profiles ==="
	$(LLVM_PROFDATA) merge -sparse $(COVERAGE_DIR)/*.profraw -o $(COVERAGE_DIR)/merged.profdata

coverage-report: | $(COVERAGE_DIR)/merged.profdata
	@echo "=== Coverage: generating reports ==="
	@# Build the -object flags for all test binaries that exist
	$(eval COV_OBJECTS := $(foreach bin,$(TEST_BINS),$(if $(wildcard $(COVERAGE_DIR)/$(TEST_SUBDIR)/$(bin)),-object $(COVERAGE_DIR)/$(TEST_SUBDIR)/$(bin))))
	@# Summary to terminal
	$(LLVM_COV) report \
		$(firstword $(COV_OBJECTS)) \
		$(wordlist 2,99,$(COV_OBJECTS)) \
		-instr-profile=$(COVERAGE_DIR)/merged.profdata \
		-ignore-filename-regex='(_deps|catch2|pugixml|nlohmann)' \
		| tee $(COVERAGE_DIR)/summary.txt
	@# HTML report
	$(LLVM_COV) show \
		$(firstword $(COV_OBJECTS)) \
		$(wordlist 2,99,$(COV_OBJECTS)) \
		-instr-profile=$(COVERAGE_DIR)/merged.profdata \
		-ignore-filename-regex='(_deps|catch2|pugixml|nlohmann)' \
		-format=html \
		-output-dir=$(COVERAGE_DIR)/html
	@echo "=== Coverage: HTML report in $(COVERAGE_DIR)/html/index.html ==="

$(COVERAGE_DIR)/merged.profdata:
	@echo "Error: coverage profile not found. Run 'make coverage-run' first."
	@exit 1

# =============================================================================
# Combined analysis target
# =============================================================================

analysis: codeql mull

# Full suite: build, test, all analysis, coverage
full: build test analysis coverage

# =============================================================================
# Python targets
# =============================================================================

python-test:
	PYTHONPATH=src uv run pytest src/Sunny.Test/Python/ -v --tb=short

python-lint:
	uv run ruff format --check .
	uv run ruff check .

python-typecheck:
	uv run mypy src/sunny --ignore-missing-imports

python-check: python-lint python-typecheck python-test

# =============================================================================
# Clean
# =============================================================================

clean-build:
	rm -rf $(BUILD_DIR)

clean-codeql:
	rm -rf $(CODEQL_DB) $(CODEQL_OUT)

clean-mull:
	rm -rf $(MULL_DIR) $(MULL_REPORT)

clean-coverage:
	rm -rf $(COVERAGE_DIR)

clean-analysis: clean-codeql clean-mull clean-coverage

clean-python:
	rm -rf .pytest_cache .mypy_cache .ruff_cache htmlcov .coverage coverage.xml
	find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true

clean: clean-build clean-analysis clean-python

# =============================================================================
# Info
# =============================================================================

info:
	@echo "Sunny Test Harness"
	@echo ""
	@echo "Directories:"
	@echo "  Build:      $(BUILD_DIR)/"
	@echo "  CodeQL DB:  $(CODEQL_DB)/"
	@echo "  CodeQL out: $(CODEQL_OUT)/"
	@echo "  Mull build: $(MULL_DIR)/"
	@echo "  Mull report:$(MULL_REPORT)/"
	@echo "  Coverage:   $(COVERAGE_DIR)/"
	@echo "  Queries:    $(ANALYSIS_DIR)/codeql/"
	@echo "  Mull config:$(ANALYSIS_DIR)/mull.yml"
	@echo ""
	@echo "Tools:"
	@echo "  CXX:         $(CXX)"
	@echo "  Mull CXX:    $(MULL_CXX)"
	@echo "  CodeQL:      $(shell $(CODEQL) --version 2>/dev/null | head -1 || echo 'not found')"
	@echo "  Mull:        $(shell $(MULL_RUNNER) --version 2>/dev/null | grep Version || echo 'not found')"
	@echo "  llvm-cov:    $(shell $(LLVM_COV) --version 2>/dev/null | head -1 || echo 'not found')"
	@echo "  Jobs:        $(JOBS)"

help:
	@echo "Sunny — Build, Test, and Analysis Harness"
	@echo ""
	@echo "Build & Test:"
	@echo "  make              Build (Release) and run all tests"
	@echo "  make build        Configure + compile"
	@echo "  make test         Run 1560 Catch2 tests"
	@echo "  make test-verbose Run tests with verbose output"
	@echo ""
	@echo "Analysis:"
	@echo "  make analysis     Run CodeQL + Mull"
	@echo "  make codeql       Static analysis (custom queries)"
	@echo "  make mull         Mutation testing (clang-18 + libc++)"
	@echo "  make coverage     Line/branch coverage (llvm-cov)"
	@echo "  make full         Build + test + analysis + coverage"
	@echo ""
	@echo "CodeQL (granular):"
	@echo "  make codeql-create    Create/refresh CodeQL database"
	@echo "  make codeql-analyze   Run queries against existing database"
	@echo ""
	@echo "Mull (granular):"
	@echo "  make mull-build       Build with Mull instrumentation"
	@echo "  make mull-run         Run mutation tests"
	@echo ""
	@echo "Coverage (granular):"
	@echo "  make coverage-build   Build with coverage instrumentation"
	@echo "  make coverage-run     Run tests and collect profiles"
	@echo "  make coverage-report  Generate HTML + summary reports"
	@echo ""
	@echo "Python:"
	@echo "  make python-test      Run Python test suite"
	@echo "  make python-lint      Ruff format + lint"
	@echo "  make python-typecheck Mypy type checking"
	@echo "  make python-check     All Python checks"
	@echo ""
	@echo "Clean:"
	@echo "  make clean            Remove all generated directories"
	@echo "  make clean-build      Remove .bin/"
	@echo "  make clean-analysis   Remove .codeql-db/ .mull-build/ .coverage-cpp/"
	@echo "  make clean-codeql     Remove CodeQL artifacts"
	@echo "  make clean-mull       Remove Mull artifacts"
	@echo "  make clean-coverage   Remove coverage artifacts"
	@echo ""
	@echo "Info:"
	@echo "  make info         Show tool versions and paths"
	@echo "  make help         Show this help"
