pin_speed: pin_speed.c
	g++ pin_speed.c -O3 -I/opt/source/pocketnc/emcapplication/include -I/usr/include/machinekit -L/opt/source/emcapplication/lib -L/usr/lib -lgpiod -lhal -lrtapi_math -lhalulapi -DULAPI -o pin_speed -lpthread 

all: pin_speed

install: all
	mkdir -p ../bin
	cp pin_speed ../bin/
