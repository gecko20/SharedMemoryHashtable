CC := clang++ # g++
CXX_FLAGS := -g -Wall -Wextra -Wconversion -pedantic -Wfatal-errors -std=c++20
LD_FLAGS := #-pthread

# Build directory
BUILD := build

# Test directory
TEST := test

all: hashtable.o server client test

hashtable.o: hashtable.cpp hashtable.h
	@mkdir -p $(BUILD)
	$(CC) $(CXX_FLAGS) -c hashtable.cpp -o $(BUILD)/$@ $(LD_FLAGS)
	#$(CC) $(CXX_FLAGS) hashtable.cpp -o $(BUILD)/$@

circular_buffer.o: circular_buffer.cpp circular_buffer.h
	@mkdir -p $(BUILD)
	$(CC) $(CXX_FLAGS) -c circular_buffer.cpp -o $(BUILD)/$@ $(LD_FLAGS)

server: hashtable.o circular_buffer.o message.h server.cpp server.h
	@mkdir -p $(BUILD)
	$(CC) $(CXX_FLAGS) server.cpp -o $(BUILD)/$@ $(BUILD)/hashtable.o $(BUILD)/circular_buffer.o $(LD_FLAGS)

client: circular_buffer.o message.h client.cpp client.h
	@mkdir -p $(BUILD)
	$(CC) $(CXX_FLAGS) client.cpp -o $(BUILD)/$@ $(BUILD)/circular_buffer.o $(LD_FLAGS)

test: hashtable.o circular_buffer.o hashtable_tests.cpp doctest.h
	@mkdir -p $(TEST)
	$(CC) $(CXX_FLAGS) hashtable_tests.cpp -o $(TEST)/$@ $(BUILD)/hashtable.o $(LD_FLAGS)
	./$(TEST)/test

run: server client
	echo "TODO"

.PHONY: clean
clean:
	@rm -rf $(BUILD)
	@rm -rf $(TEST)

