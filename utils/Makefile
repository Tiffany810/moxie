libs := -lpthread
cflags := -std=c++11 -static -Wall -DNDEBUG
CC = g++

moxie_root := ../../

objects := $(patsubst %.cpp, %.o, $(wildcard *.cpp))
headers := $(wildcard *.h)

utils:$(objects)
	ar -crv libutils.a $^
	mv libutils.a ../lib
%.o:%.cpp $(headers)
	$(CC) -c $<  -I${moxie_root} -I./ $(cflags) -o $@  $(libs)

.PHONY : clean
clean:
	rm *.o -rf 
