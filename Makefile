CXX=g++
CFLAGS=-std=c++11 -DDEBUG -g
SRC=$(wildcard *.cpp)
DEP=$(SRC) $(wildcard *.h)
OUTPUT=FogNode
UID=$(shell id -u)

all: $(DEP)
	$(CXX) $(CFLAGS) $(SRC) -o $(OUTPUT)

clean:
	@rm -f $(OUTPUT)
	@rm -rf logs
	@rm -rf testconfig.txt

testlocal:
	@mkdir -p logs
	./$(OUTPUT) 50 1 localhost 6000 6000 localhost 6001 localhost 6002 > logs/node1.log 2>&1 &
	./$(OUTPUT) 40 2 localhost 6001 6001 localhost 6000 localhost 6002 > logs/node2.log 2>&1 &
	./$(OUTPUT) 30 3 localhost 6002 6002 localhost 6000 localhost 6001 > logs/node3.log 2>&1 &
	java -jar "IoTNodeReqGen/IoTNodeReqGen.jar" "IoTNodeReqGen/config.txt" > logs/gen.log 2>&1 &

testnet:
	./gentester.py | /bin/sh

kill:
	@pkill -9 $(OUTPUT) -U $(UID); pkill -9 java -U $(UID)

inspect:
	@./inspect.py "$(SEARCH)" logs/*.log
	
log:
	@gedit logs/*.log > /dev/null 2>&1 &

