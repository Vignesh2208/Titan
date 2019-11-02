home_dir=$(HOME)
tracer_dir=${home_dir}/Titan/src/tracer
test_dir=${tracer_dir}/tests/thread-test

clean:
	@rm -f ${test_dir}/cmds.txt > /dev/null
	@rm -f ${test_dir}/thread_test > /dev/null


build:
	@echo "Building Thread Test ..."
	@gcc -w ${test_dir}/thread_test.c -o ${test_dir}/thread_test -lpthread
	@echo ${test_dir}/thread_test > ${test_dir}/cmds.txt
	chmod 777 ${test_dir}/thread_test

run:
	@echo "Starting Thread Test ..."
	python ${home_dir}/Titan/examples/example_vt_experiment.py --cmds_to_run_file=${test_dir}/cmds.txt
	@echo "Thread Test Finished ..."

run_python:
	@echo "Starting Thread Test ..."
	@echo "python ${test_dir}/thread.py" > ${test_dir}/cmds.txt
	python ${home_dir}/Titan/examples/example_vt_experiment.py --cmds_to_run_file=${test_dir}/cmds.txt
	@echo "Thread Test Finished ..."


start: run

start_python: run_python
