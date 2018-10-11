libs := -lmoxie -lmcached -lprotobuf -ltcmalloc_minimal -lpthread 
cflags := -O2 -std=c++11 -static -Wall
CC = g++

LDDIR += -L../../lib \
		 -L./mcached \
		 -L/opt/protobuf/lib \
		 -L/opt/gperftools-2.7/lib

INCLUDEDIR += -I./ \
			  -I./mcached \
			  -I../../base \
			  -I/opt/protobuf/include

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
	rm *.o -rf
