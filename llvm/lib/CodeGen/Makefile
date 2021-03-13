build:
	clang -fno-rtti -fPIC -shared -o libvtlooppass.so VirtualTimeLoopIRPass.cpp -mllvm --no-vt-insertion
	opt -load ./libvtlooppass.so -vtlooppass ../../../scripts/loop_example.bc -o loop_example.bc
	llc -filetype=asm loop_example.bc -o loop_example.s
	clang -lvtllogic loop_example.s -o loop_example


clean:
	rm *.so
	rm *.json
	rm *.bc
	rm *.s
	rm test
