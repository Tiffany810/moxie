include ../config.mk
libs := -lhttp -lutils -lmoxie -lpthread
cflags := -O2 -std=c++11 -static -Wall
CC = g++

moxie_base := ../base
moxie_http := ../http
moxie_libs := ../lib

base_srcs := $(wildcard ${moxie_base}/*.cpp)
base_headers := $(wildcard ${moxie_base}/*.h)
http_srcs := $(wildcard ${moxie_http}/*.cpp)
http_headers := $(wildcard ${moxie_http}/*.h)

moxie:${base_srcs} ${base_headers} ${http_srcs} ${http_headers}
	$(CC) Echo.cpp  -I${moxie_base} -I${moxie_http} -I./ -L${moxie_libs} $(cflags) -o Example_bin_Echo  $(libs)
	$(CC) MultiThreadEcho.cpp  -I${moxie_base} -I${moxie_http} -I./ -L${moxie_libs} $(cflags) -o Example_bin_MultiThreadEcho  $(libs)
	$(CC) TimerExam.cpp  -I${moxie_base} -I${moxie_http} -I./ -L${moxie_libs} $(cflags) -o Example_bin_TimerExam  $(libs)
	$(CC) EventLoopThreadExam.cpp  -I${moxie_base} -I${moxie_http} -I./ -L${moxie_libs} $(cflags) -o Example_bin_EventLoopThreadExam  $(libs)
	$(CC) Http.cpp  -I${moxie_base} -I${moxie_http} -I./ -L${moxie_libs} $(cflags) -o Example_bin_Http  $(libs)
.PHONY : clean
clean:
	rm *.o -rf
	rm Example_bin_* -rf
