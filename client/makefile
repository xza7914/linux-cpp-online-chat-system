TRA = mycli.out
SOUR = mycli.cpp

$(TRA):$(SOUR)
	g++ $(SOUR) -o $(TRA) -lpthread -lmysqlclient -I/usr/include/mysql/ -L/usr/lib/mysql
	
.PHONY:
clear:
	rm $(TRA)
