# BlackoutAvoider -- build & run automation (C++17)
# Usage: make <target>

CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Isrc

.PHONY: all build test validate run bench clean help

all: build

help:
	@echo "Targets:"
	@echo "  make build     - compile the solver  -> ./blackout"
	@echo "  make run        - solve every lineup"
	@echo "  make validate   - cross-check the DP against brute force on all lineups"
	@echo "  make bench       - runtime/memory benchmark on the representative lineup"
	@echo "  make test        - build and run the automated test suite"
	@echo "  make clean       - remove build artifacts"

build: blackout

blackout: src/main.cpp src/blackout.hpp src/lineups.hpp
	$(CXX) $(CXXFLAGS) -o blackout src/main.cpp

run: build
	./blackout

validate: build
	./blackout --validate

bench: build
	./blackout --bench

test: test_blackout
	./test_blackout

test_blackout: tests/test_blackout.cpp src/blackout.hpp src/lineups.hpp
	$(CXX) $(CXXFLAGS) -o test_blackout tests/test_blackout.cpp

clean:
	rm -f blackout test_blackout
