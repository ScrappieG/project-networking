COMPILER := g++
FLAGS := -std=c++20 -O2 -pthread
DIR := ./src/

SRC := $(DIR)main.cpp $(DIR)config.cpp $(DIR)logger.cpp $(DIR)peer.cpp
OBJ :=  $(SRC:.cpp=.o)

peerProcess: $(OBJ)
	$(COMPILER) $(FLAGS) -o $@ $(OBJ)

%.o: %.cpp
	$(COMPILER) $(FLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) peerProcess
