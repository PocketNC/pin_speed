all:
	g++ pin_speed.c -O3 -I/home/pocketnc/EMCApplication/include -I/usr/include/machinekit -L/home/pocketnc/EMCApplication/lib -L/usr/lib -lgpiod -lhal -lrtapi_math -lhalulapi -DULAPI -o pin_speed -lpthread 
