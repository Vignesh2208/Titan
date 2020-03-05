build: 
	@mkdir -p bin
	@gcc -o bin/x64_synchronizer x64_synchronizer.c
	@clang -o bin/x86_vt_spinner -lvtllogic x86_vt_spinner.c 
	@chmod 777 bin/x64_synchronizer
	@cp bin/x64_synchronizer /bin/
	@cp bin/x86_vt_spinner /bin/
	@chmod 777 /bin/x86_vt_spinner

clean:
	@rm -f /bin/x64_synchronizer
	@rm -f /bin/x86_vt_spinner
	@rm -f bin/*
	@touch bin/README
	@chmod 777 bin/README
