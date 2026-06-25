# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread

# Executable names
SERVER_BIN = redis-server
BENCH_BIN = redis-bench

# Object files needed for the server
SERVER_OBJS = main.o server.o store.o persistence.o wal.o

# Object files needed for the benchmark tool
BENCH_OBJS = benchmark.o

# Default target: builds both the server and the benchmark
all: $(SERVER_BIN) $(BENCH_BIN)

# Link the server
$(SERVER_BIN): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $(SERVER_BIN) $(SERVER_OBJS)

# Link the benchmark tool
$(BENCH_BIN): $(BENCH_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BENCH_BIN) $(BENCH_OBJS)

# Compile C++ source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up compiled files and data dumps
clean:
	rm -f *.o $(SERVER_BIN) $(BENCH_BIN) *.dat database_state.dat