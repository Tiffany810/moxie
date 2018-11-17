libs := -ljsoncpp -lcurl -lhttp -lutils -lmoxie -lfloyd -lslash -lpink -lmcached -lprotobuf -lrocksdb -ltcmalloc_minimal -lpthread  
cflags := -O2 -g -std=c++11 -static -Wall
#cflags := -O2 -g -std=c++11 -static -Wall -DNDEBUG
CC = g++

LDDIR += -L../../lib \
		 -L./mcached \
		 -L/opt/curl/lib \
		 -L/opt/protobuf/lib \
		 -L/opt/rocksdb/lib \
		 -L/opt/jsoncpp/lib \
		 -L/opt/gperftools-2.7/lib \
		 -L./mraft/floyd/lib \
		 -L./mraft/floyd/third/ \
		 -L./mraft/floyd/third/mcached \
		 -L./mraft/floyd/third/pink/pink/lib \
		 -L./mraft/floyd/third/slash/slash/lib \
		 -L./mraft/floyd/third/moxie/lib

INCLUDEDIR += -I./ \
			  -I../ \
			  -I../../ \
			  -I../../http \
			  -I../../utils \
			  -I../../base \
			  -I/opt/curl/include \
			  -I/opt/protobuf/include \
			  -I/opt/rocksdb/include/ \
			  -I/opt/jsoncpp/include \
			  -I./mraft \
			  -I./mraft/floyd \
			  -I./mraft/floyd/include \
			  -I./mraft/floyd/third \
			  -I./mraft/floyd/third/mcached \
			  -I./mraft/floyd/third/pink \
			  -I./mraft/floyd/third/slash \
			  -I./mraft/floyd/third/moxie/base \
			  -I./mraft/floyd/third/moxie/utils

moxie_base := ../../base
moxie_libs := ../../base

objects := $(patsubst %.cpp, %.o, $(wildcard *.cpp))
headers := $(wildcard *.h)

all:
	make -C mraft/floyd
	make -C mcached
	make Mcached
Mcached:$(objects)
	$(CC) $^ -o $@ $(LDDIR) $(libs)
%.o:%.cpp $(headers)
	echo $(LDDIR)
	$(CC) -c $< $(INCLUDEDIR) -I./ $(cflags) -o $@  

.PHONY : clean
clean:
	make clean -C mraft/floyd
	make clean -C mcached
	rm *.o -rf
