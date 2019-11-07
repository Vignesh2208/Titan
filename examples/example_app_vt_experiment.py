import sys
import os
import time
import vt_functions as kf
import sys
import argparse


def start_new_dilated_process(tracer_id, total_num_tracers, log_file_fd):
    newpid = os.fork()
    if newpid == 0:
        os.dup2(log_file_fd, sys.stdout.fileno())
        os.dup2(log_file_fd, sys.stderr.fileno())
        args = ["app_vt_test_tracer", str(total_num_tracers), str(tracer_id)]
        os.execvp(args[0], args)
    else:
        return newpid


def main():

    
    parser = argparse.ArgumentParser()



    parser.add_argument('--num_insns_per_round', dest='num_insns_per_round',
                        help='Number of insns per round', type=int,
                        default=1000)

    parser.add_argument('--num_progress_rounds', dest='num_progress_rounds',
                        help='Number of rounds to run', type=int,
                        default=1000)

    parser.add_argument('--num_tracers', dest='num_tracers',
                        help='Number of tracers to run', type=int,
                        default=1)

    args = parser.parse_args()
    
    log_fds = []
    tracer_pids = []
    cmds_to_run = []

    raw_input('Press any key to continue !')

    num_tracers = args.num_tracers
    for i in xrange(0, num_tracers) :
        with open("/tmp/tracer_log%d.txt" %(i), "w") as f:
            pass
    log_fds = [ os.open("/tmp/tracer_log%d.txt" %(i), os.O_RDWR | os.O_CREAT ) \
        for i in xrange(0, num_tracers) ]

    print "Initializing VT Module !"     
    if kf.initializeExp(num_tracers) < 0 :
        print "VT module initialization failed ! Make sure you are running\
            the dilated kernel and kronos module is loaded !"
        sys.exit(0)

    raw_input('Press any key to continue !')
    
    print "Starting all commands to run !"
    
    for i in xrange(0, num_tracers):
        print "Starting tracer: %d" %(i + 1)
        start_new_dilated_process(i + 1, num_tracers, log_fds[i])
    
    print "Synchronizing anf freezing tracers ..."
    while kf.synchronizeAndFreeze() <= 0:
        print "VT Module >> Synchronize and Freeze failed. Retrying in 1 sec"
        time.sleep(1)

    raw_input('Press any key to continue !')


    print "Starting Synchronized Experiment !"
    start_time = float(time.time())
    if args.num_progress_rounds > 0 :
        print "Running for %d rounds ... " %(args.num_progress_rounds)
        num_finised_rounds = 0
        step_size = 1
        while num_finised_rounds < args.num_progress_rounds:
            kf.progressBy(args.num_insns_per_round, step_size)
            num_finised_rounds += step_size
            print "Ran %d rounds ..." %(num_finised_rounds)

    elapsed_time = float(time.time()) - start_time
    print "Total time elapsed (secs) = ", elapsed_time
    raw_input("Press Enter to continue...")
    print "Stopping Synchronized Experiment !"
    kf.stopExp()

    for fd in log_fds:
        os.close(fd)

    print "Finished ! Logs of each ith tracer can be found in /tmp/tracer_logi.txt"
    
            

if __name__ == "__main__":
	main()
