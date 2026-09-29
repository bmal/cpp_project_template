# Convenience wrapper: every target calls one preset, CMake target, or script. Run `make help`.
# A new workflow preset needs one line here: `<preset>: workflow/<preset> ## <description>`.

PRESET ?= dev
BASE ?= main
FUZZ_SECONDS ?= 30

.DEFAULT_GOAL := help
.PHONY: help bootstrap bench bench-compare fuzz build test clean-all

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
release: workflow/release ## Configure, build, and test the optimized build
asan: workflow/asan ## Configure, build, and test under AddressSanitizer and UBSan
tsan: workflow/tsan ## Configure, build, and test under ThreadSanitizer
msan: workflow/msan ## Configure, build, and test under MemorySanitizer; Linux, after scripts/build-msan-libcxx.sh
coverage: workflow/coverage ## Configure, build, and test with coverage instrumentation

bench: ## Build the bench preset and run every benchmark, writing JSON to build/bench/bench/
	cmake --preset bench
	cmake --build --preset bench --target run_benchmarks

bench-compare: ## Run the benchmarks at BASE (default main) and here, and print the difference
	scripts/bench-compare.sh $(BASE)

fuzz: ## Build the fuzz preset and fuzz every harness for FUZZ_SECONDS (default 30) each
	cmake --preset fuzz -DPROJECT_FUZZ_SECONDS=$(FUZZ_SECONDS)
	cmake --build --preset fuzz --target run_fuzz

build: ## Build PRESET (default dev) after it has been configured
	cmake --build --preset $(PRESET)

test: ## Run the test preset PRESET (default dev) after it has been built
	ctest --preset $(PRESET)

clean-all: ## Delete every build directory
	rm -rf build

workflow/%:
	cmake --workflow --preset $*
