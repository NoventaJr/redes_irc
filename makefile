compile_server:
	gcc server.c converter.c common.c -I Include -o server -lm -g -lpthread

run_server:
	./server 8080

compile_client:
	gcc client.c converter.c common.c -I Include -o client -lm -g -lpthread

run_client:
	./client 8080