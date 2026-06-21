CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I src

SRCS = src/main.cpp \
       src/parser/lexer.cpp \
       src/parser/parser.cpp \
       src/storage/DiskManager.cpp \
       src/storage/BufferPool.cpp \
       src/storage/PageManager.cpp \
       src/index/BPlusTree.cpp \
       src/query/Executor.cpp \
       src/optimizer/Optimizer.cpp \
       src/transaction/LockManager.cpp \
       src/transaction/Transaction.cpp \
       src/recovery/LogManager.cpp \
       src/recovery/Recovery.cpp

TARGET = minidb

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) -lpthread

clean:
	rm -f $(TARGET)
	rm -rf minidb_data

.PHONY: all clean
