CC := clang++
CXX_FLAGS := -g -O0 -Wall -Wextra -Wconversion -pedantic -Wfatal-errors -std=c++20
LD_FLAGS := -pthread

# Build directory
BUILD := build

# Test directory
TEST := test

#all: hashtable.o server client test

#hashtable.o: hashtable.cpp hashtable.h
#	@mkdir -p $(BUILD)
#	$(CC) $(CXX_FLAGS) -c hashtable.cpp -o $(BUILD)/$@ $(LD_FLAGS)
#
#mutex.o: mutex.cpp mutex.h
#	@mkdir -p $(BUILD)
#	$(CC) $(CXX_FLAGS) -c mutex.cpp -o $(BUILD)/$@ $(LD_FLAGS)
#
#circular_buffer.o: circular_buffer.cpp circular_buffer.h mutex.o
#	@mkdir -p $(BUILD)
#	$(CC) $(CXX_FLAGS) -c circular_buffer.cpp -o $(BUILD)/$@ $(BUILD)/mutex.o $(LD_FLAGS)

#server: hashtable.o mutex.o circular_buffer.o message.h server.cpp server.h
#	@mkdir -p $(BUILD)
#	$(CC) $(CXX_FLAGS) server.cpp -o $(BUILD)/$@ $(BUILD)/hashtable.o $(BUILD)/mutex.o $(BUILD)/circular_buffer.o $(LD_FLAGS)
#
#client: circular_buffer.o mutex.o message.h client.cpp client.h
#	@mkdir -p $(BUILD)
#	$(CC) $(CXX_FLAGS) client.cpp -o $(BUILD)/$@ $(BUILD)/mutex.o $(BUILD)/circular_buffer.o $(LD_FLAGS)
#
#test: hashtable.o mutex.o circular_buffer.o hashtable_tests.cpp doctest.h
#	@mkdir -p $(TEST)
#	$(CC) $(CXX_FLAGS) hashtable_tests.cpp -o $(TEST)/$@ $(BUILD)/mutex.o $(BUILD)/hashtable.o $(LD_FLAGS)
#	./$(TEST)/test

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
	./$(TEST)/test

run: server client
	echo "TODO"

clean:
	@rm -rf $(BUILD)
	@rm -rf $(TEST)

.PHONY: all clean test

