build: 
	@mkdir -p bin
	@gcc -o bin/x64_synchronizer x64_synchronizer.c
	@chmod 777 bin/x64_synchronizer
	@cp bin/x64_synchronizer /bin/
	

clean:
	@rm /bin/x64_synchronizer
	@rm -f bin/*
	@touch bin/README
	@chmod 777 bin/README