libs := -ljsoncpp -lcurl -lhttp -lutils -lmoxie -lmcached -lprotobuf -ltcmalloc_minimal -lpthread 
cflags := -O2 -std=c++11 -static -Wall -DNDEBUG
CC = g++

LDDIR += -L../../lib \
		 -L./mcached \
		 -L/opt/protobuf/lib \
		 -L/opt/rocksdb/lib \
		 -L/opt/jsoncpp/lib \
		 -L/opt/gperftools-2.7/lib

INCLUDEDIR += -I./ \
			  -I../ \
			  -I../../ \
			  -I../../http \
			  -I../../utils \
			  -I../../base \
			  -I/opt/protobuf/include \
			  -I/opt/rocksdb/include/ \
			  -I/opt/jsoncpp/include

moxie_base := ../../base
moxie_libs := ../../base

objects := $(patsubst %.cpp, %.o, $(wildcard *.cpp))
headers := $(wildcard *.h)

all:
	make -C mcached
	make Mcached
Mcached:$(objects)
	$(CC) $^ -o $@ $(LDDIR) $(libs)
%.o:%.cpp $(headers)
	echo $(LDDIR)
	$(CC) -c $< $(INCLUDEDIR) -I./ $(cflags) -o $@  

.PHONY : clean
clean:
	make clean -C mcached
	rm *.o -rf
