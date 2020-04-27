build: 
	@mkdir -p bin
	@gcc -o bin/x64_synchronizer x64_synchronizer.c
	@clang -o bin/x86_vt_spinner -lvtllogic x86_vt_spinner.c 
	@clang -o bin/timerfd_example -lvtllogic timerfd_example.c
	@chmod 777 bin/x64_synchronizer
	@chmod 777 bin/timerfd_example
	@cp bin/x64_synchronizer /bin/
	@cp bin/x86_vt_spinner /bin/
	@chmod 777 /bin/x86_vt_spinner

clean:
	@rm -f /bin/x64_synchronizer
	@rm -f /bin/x86_vt_spinner
	@rm -f bin/*
	@touch bin/README
	@chmod 777 bin/README
