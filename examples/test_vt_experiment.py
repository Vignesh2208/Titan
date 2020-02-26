import sys
import os
import time
import vt_functions as kf
import sys
import argparse

EXP_CBE = 1
EXP_CS = 2

def start_new_dilated_process(tracer_id, timeline_id, cmd_to_run, log_file_fd,
    exp_type):
    
    newpid = os.fork()
    if newpid == 0:
        os.dup2(log_file_fd, sys.stdout.fileno())
        os.dup2(log_file_fd, sys.stderr.fileno())
        args = ["tracer", "-t", str(timeline_id), "-i", str(tracer_id),
                "-c", cmd_to_run, "-e", str(exp_type)]
        os.execvp(args[0], args)
    else:
        return newpid


def main():

    
    parser = argparse.ArgumentParser()


    parser.add_argument('--cmds_to_run_file', dest='cmds_to_run_file',
                        help='path to file containing commands to run', \
                        type=str, default='cmds_to_run_file.txt')


    parser.add_argument('--num_insns_per_round', dest='num_insns_per_round',
                        help='Number of insns per round', type=int,
                        default=100000)

    parser.add_argument('--num_progress_rounds', dest='num_progress_rounds',
                        help='Number of rounds to run', type=int,
                        default=1)

    parser.add_argument('--exp_type', dest='exp_type',
        help='Number of rounds to run', type=int, default=EXP_CBE)

    args = parser.parse_args()
    log_fds = []
    tracer_pids = []
    cmds_to_run = []


    if not os.path.isfile(args.cmds_to_run_file):
        print "Commands file path is incorrect !"
        sys.exit(0)
    fd1 = open(args.cmds_to_run_file, "r")
    cmds_to_run = [x.strip() for x in fd1.readlines()]
    fd1.close()
    for i in xrange(0, len(cmds_to_run)) :
        with open("/tmp/tracer_log_%d.txt" %(i), "w") as f:
            pass
    log_fds = [ os.open("/tmp/tracer_log_%d.txt" %(i), os.O_RDWR | os.O_CREAT ) \
        for i in xrange(0, len(cmds_to_run)) ]
    num_tracers = len(cmds_to_run)
    num_timelines = len(cmds_to_run)

    raw_input('Press any key to continue !')
    for i in xrange(0, num_tracers) :
        with open("/tmp/tracer_log_%d.txt" %(i), "w") as f:
            pass
    log_fds = [ os.open("/tmp/tracer_log_%d.txt" %(i), os.O_RDWR | os.O_CREAT ) \
        for i in xrange(0, num_tracers) ]

    print "Initializing VT Module !"     
    if kf.initializeVTExp(args.exp_type, num_timelines, num_tracers) < 0 :
        print "VT module initialization failed ! Make sure you are running\
               the dilated kernel and kronos module is loaded !"
        sys.exit(0)

    raw_input('Press any key to continue !')
    
    print "Starting all commands to run !"
    
    for i in xrange(0, num_tracers):
        print "Starting tracer: %d" %(i + 1)
        start_new_dilated_process(i + 1, i, cmds_to_run[i], log_fds[i],
                                  args.exp_type)
    
    print "Synchronizing anf freezing tracers ..."
    while kf.synchronizeAndFreeze() <= 0:
        print "VT Module >> Synchronize and Freeze failed. Retrying in 1 sec"
        time.sleep(1)

    raw_input('Press any key to continue !')


    print "Starting Synchronized Experiment !"
    start_time = float(time.time())
    if args.num_progress_rounds > 0 :
        print "Running for %d rounds ... " %(args.num_progress_rounds)
        num_finished_rounds = 0
        step_size = min(1, args.num_progress_rounds)
        while num_finished_rounds < args.num_progress_rounds:

            if args.exp_type == EXP_CBE:
                kf.progressBy(args.num_insns_per_round, step_size)

            num_finished_rounds += step_size
            print "Ran %d rounds ..." %(num_finished_rounds), " elapsed time ...", float(time.time()) - start_time
            time.sleep(0.1)
            raw_input("Press Enter to continue...")

    elapsed_time = float(time.time()) - start_time
    print "Total time elapsed (secs) = ", elapsed_time
    raw_input("Press Enter to continue...")
    print "Stopping Synchronized Experiment !"
    kf.stopExp()

    for fd in log_fds:
        os.close(fd)

    print "Finished ! Logs of each ith tracer can be found in /tmp/tracer_log_i.txt"
    
            

if __name__ == "__main__":
	main()
