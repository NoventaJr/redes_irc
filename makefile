all: compile_server run

compile_server:
	gcc server.c converter.c -I Include -o server -lm -g

run_server:
	./server 8080

compile_client:
	gcc client.c converter.c -I Include -o client -lm -g

run_client:
	./client 8080

clean:
	rm ./server