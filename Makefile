CC := clang++
CXX_FLAGS := -g -O2 -Wall -Wextra -Wconversion -pedantic -Wfatal-errors -std=c++20
LD_FLAGS := -pthread

# Build directory
BUILD := build

# Test directory
TEST := test


#all: hashtable.o mutex.o circular_buffer.o server client test
all: server client test

hashtable.o: hashtable.cpp hashtable.h
	@mkdir -p $(BUILD)
	$(CC) $(CXX_FLAGS) -c $< -o $(BUILD)/$@ $(LD_FLAGS)

mutex.o: mutex.cpp mutex.h
	@mkdir -p $(BUILD)
	$(CC) $(CXX_FLAGS) -c $< -o $(BUILD)/$@ $(LD_FLAGS)

circular_buffer.o: circular_buffer.cpp circular_buffer.h mutex.h
	@mkdir -p $(BUILD)
	$(CC) $(CXX_FLAGS) -c $< -o $(BUILD)/$@ $(LD_FLAGS)

server.o: server.cpp server.h
	@mkdir -p $(BUILD)
	$(CC) $(CXX_FLAGS) -c $< -o $(BUILD)/$@ $(LD_FLAGS)

server: server.o server.h hashtable.o mutex.o circular_buffer.o
	$(CC) $(BUILD)/mutex.o $(BUILD)/circular_buffer.o $(BUILD)/server.o -o $(BUILD)/$@ $(LD_FLAGS)

client.o: client.cpp client.h #mutex.o circular_buffer.o
	@mkdir -p $(BUILD)
	$(CC) $(CXX_FLAGS) -c $< -o $(BUILD)/$@ $(LD_FLAGS)

client: client.o client.h mutex.o circular_buffer.o
	$(CC) $(BUILD)/mutex.o $(BUILD)/circular_buffer.o $(BUILD)/client.o -o $(BUILD)/$@ $(LD_FLAGS)

test: hashtable.o mutex.o circular_buffer.o hashtable_tests.cpp doctest.h
	@mkdir -p $(TEST)
	$(CC) $(CXX_FLAGS) hashtable_tests.cpp -o $(TEST)/$@ $(BUILD)/hashtable.o $(BUILD)/mutex.o $(BUILD)/circular_buffer.o $(LD_FLAGS)
	./$(TEST)/test -d

run: server client
	echo "Spawning a server in the background and a client in the foreground."
	./$(BUILD)/server 10 > /dev/null &
	@sleep 2
	./$(BUILD)/client
	@kill -s INT `pgrep server`

clean:
	@rm -rf $(BUILD)
	@rm -rf $(TEST)

.PHONY: all clean test

