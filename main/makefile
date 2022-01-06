# makefile
CC = g++
TAR = main.out
MYSQL = -lmysqlclient -I/usr/include/mysql/ -L/usr/lib/mysql
PTHREAD = -lpthread
OBJ = main.o global.h locker.h request.h server.h threadpool.h tools.h user.h

vpath %.h ../header

$(TAR):$(OBJ)
	$(CC) $^ -o $@ $(MYSQL) $(PTHREAD)

main.o:main.cpp
	$(CC) -o $@ -c $<
