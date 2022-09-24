CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.o  ./timer/lst_timer.o ./http/http_conn.o ./log/log.o ./CGImysql/sql_connection_pool.o  webserver.o config.o
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm  -r server *.o
