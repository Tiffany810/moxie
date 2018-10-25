libs := -lpthread
cflags := -g -std=c++11 -static -Wall -DNDEBUG
CC = g++

moxie_root := ../
moxie_base := ../base

objects := $(patsubst %.cpp, %.o, $(wildcard *.cpp))
headers := $(wildcard *.h)

http:$(objects)
	ar -crv libhttp.a $^
	mv libhttp.a ../lib
%.o:%.cpp $(headers)
	$(CC) -c $<  -I${moxie_root} -I${moxie_base} -I./ $(cflags) -o $@  $(libs)

.PHONY : clean
clean:
	rm *.o -rf 
