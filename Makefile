# Convenience wrapper: every target calls one preset, CMake target, or script. Run `make help`.
# A new workflow preset needs one line here: `<preset>: workflow/<preset> ## <description>`.

PRESET ?= dev
BASE ?= main
FUZZ_SECONDS ?= 30

.DEFAULT_GOAL := help
.PHONY: help bootstrap coverage bench bench-compare fuzz lint format format-check build test clean-all

help: ## List every target
	@echo "Usage: make <target> [PRESET=<preset>] [BASE=<git ref>] [FUZZ_SECONDS=<n>]"
	@echo
	@sed -n 's/^\([A-Za-z0-9][A-Za-z0-9_.-]*\):.*## \(.*\)/\1|\2/p' $(MAKEFILE_LIST) | \
		awk -F '|' '{ printf "  %-14s %s\n", $$1, $$2 }'

bootstrap: ## Install the toolchain, tools, and vcpkg once per machine
	scripts/bootstrap.sh

dev: workflow/dev ## Configure, build, and run unit and integration tests of the debug build
functional: workflow/functional ## Build dev and run the black-box tests of the apps
stress: workflow/stress ## Build dev and run the long-running tests labeled stress
release: workflow/release ## Configure, build, and test the optimized build, with LTO, for this machine's CPU
relwithdebinfo: workflow/relwithdebinfo ## Configure, build, and test at -O2 with debug info and frame pointers
profile: workflow/profile ## Configure, build, and test the release flags with debug info and no LTO, for perf
cxx26: workflow/cxx26 ## Configure, build, and run unit and integration tests in C++26
asan: workflow/asan ## Configure, build, and test under AddressSanitizer and UBSan
tsan: workflow/tsan ## Configure, build, and test under ThreadSanitizer
msan: workflow/msan ## Configure, build, and test under MemorySanitizer; Linux, after scripts/build-msan-libcxx.sh

coverage: ## Configure the coverage preset, run its tests, and write build/current/coverage/lcov.info
	cmake --preset coverage
	cmake --build --preset coverage --target coverage

bench: ## Build the bench preset and run every benchmark, writing JSON to build/current/bench/
	cmake --preset bench
	cmake --build --preset bench --target run_benchmarks

bench-compare: ## Run the benchmarks at BASE (default main) and here, and print the difference
	scripts/bench-compare.sh $(BASE)

fuzz: ## Build the fuzz preset and fuzz every harness for FUZZ_SECONDS (default 30) each
	cmake --preset fuzz -DPROJECT_FUZZ_SECONDS=$(FUZZ_SECONDS)
	cmake --build --preset fuzz --target run_fuzz

lint: ## Configure PRESET (default dev) and run clang-tidy over it; any finding fails
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET) --target lint

format: ## Rewrite every C++ and CMake file with clang-format and gersemi
	scripts/format.sh

format-check: ## Fail on any C++ or CMake file that make format would change; changes nothing
	scripts/format.sh --check

build: ## Build PRESET (default dev) after it has been configured
	cmake --build --preset $(PRESET)

test: ## Run the test preset PRESET (default dev) after it has been built
	ctest --preset $(PRESET)

clean-all: ## Delete every build directory
	rm -rf build

workflow/%:
	cmake --workflow --preset $*
