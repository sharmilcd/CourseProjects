CXX      := g++
CXXFLAGS := -std=c++11 -O2 -Iinclude
TARGET   := rip
SRCS     := src/main.cpp src/routing_algo.cpp
HDRS     := include/node.h

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $@

# Regenerate every reference output under tests/expected/ and diff against
# what is committed. Exits non-zero if any topology drifts.
test: $(TARGET)
	@./scripts/run_tests.sh

clean:
	rm -f $(TARGET) $(TARGET).exe
