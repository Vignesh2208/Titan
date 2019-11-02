home_dir=$(HOME)
tracer_dir=${home_dir}/Titan/src/tracer
test_dir=${tracer_dir}/tests/socket-test

clean:
	@rm -f ${test_dir}/cmds.txt > /dev/null

build:
	@echo "Building Socket Test ..."
	@echo "python ${test_dir}/udp_server.py\npython ${test_dir}/udp_client.py" > ${test_dir}/cmds.txt

run:
	@echo "Starting Socket test ..."
	python ${home_dir}/Titan/examples/example_vt_experiment.py --cmds_to_run_file=${test_dir}/cmds.txt
	@echo "Socket Test Finished ..."

start: run
