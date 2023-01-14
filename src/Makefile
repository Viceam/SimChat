all: test_server.cpp server.cpp server.h test_client.cpp client.cpp client.h global.h
	g++ -o client test_client.cpp client.cpp -lpthread
	g++ -o server test_server.cpp server.cpp -lpthread -lmysqlclient -lhiredis
test_server: test_server.cpp server.cpp server.h global.h
	g++ -o server test_server.cpp server.cpp -lpthread -lmysqlclient -lhiredis
test_client: test_client.cpp client.cpp client.h global.h
	g++ -o client test_client.cpp client.cpp -lpthread
clean:
	rm server
	rm client
