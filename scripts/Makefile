build: 
	@mkdir -p bin
	@gcc -o bin/x64_synchronizer x64_synchronizer.c
	@clang -O1 -o bin/x86_vt_spinner -lvtllogic x86_vt_spinner.c 
	@clang -O1 -o bin/timerfd_example -lvtllogic timerfd_example.c
	@clang -O1 -o bin/tcp_server -lvtllogic tcp_server.c
	@clang -O1 -o bin/tcp_client -lvtllogic tcp_client.c
	@clang -O1 -lvtllogic -o bin/loop_example loop_example.c #-O1 flag here is crucial to enable loop optimization passes and loop set lookahead
	@chmod -R 777 bin/*
	@cp bin/x64_synchronizer /bin/
	@cp bin/x86_vt_spinner /bin/

clean:
	@rm -f bin/*
	@rm -f *.bc
	@rm -f *.ll
	@rm -f *.artifacts.json
	@rm -f *.s
	@touch bin/README
	@chmod 777 bin/README
