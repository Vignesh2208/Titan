#!/usr/bin/env python
"""ttn script to initialize titan-projects."""

import os
import sys
import argparse
import logging
import tools.ttn.ttn_constants as c
import tools.ttn.ttn_ops as ops


logging.basicConfig(format='%(message)s', level=logging.NOTSET)

parser = argparse.ArgumentParser(description='sets up/initializes a new '
                                             'titan-supported project for '
                                             'compilation with clang-vt')
parser.add_argument('--project_name', type=str, default=c.DEFAULT_PROJECT_NAME,
                    help='Name of the project')
parser.add_argument('--arch', type=str, default=c.NO_ARCH,
                    help='Machine architecture type for project compilation')
parser.add_argument('--project_src_dir', type=str,
                    default=c.DEFAULT_PROJECT_SRC_DIR,
                    help='Directory containing project source')

parser.add_argument('--l1_ins_cache_size_kb', type=int,
                    default=c.DEFAULT_L1_INS_CACHE_SIZE_KB,
                    help='Instruction cache-size in KB')
parser.add_argument('--l1_ins_cache_lines', type=int,
                    default=c.DEFAULT_L1_INS_CACHE_LINE_SIZE_BYTES,
                    help='Number of instruction cache-lines')
parser.add_argument('--l1_ins_cache_policy', type=str,
                    default=c.DEFAULT_L1_INS_CACHE_REPLACEMENT_POLICY,
                    help='Instruction cache type')
parser.add_argument('--l1_ins_cache_miss_cycles', type=int,
                    default=c.DEFAULT_L1_INS_CACHE_MISS_CYCLES,
                    help='Number of cycles spent in the event of a cache miss')
parser.add_argument('--l1_ins_cache_assoc', type=int,
                    default=c.DEFAULT_L1_INS_CACHE_ASSOCIATIVITY,
                    help='L1 ins cache associativity')
parser.add_argument('--l1_data_cache_size_kb', type=int,
                    default=c.DEFAULT_L1_DATA_CACHE_SIZE_KB,
                    help='Data cache-size in KB')
parser.add_argument('--l1_data_cache_lines', type=int,
                    default=c.DEFAULT_L1_DATA_CACHE_LINE_SIZE_BYTES,
                    help='Number of data cache-lines')
parser.add_argument('--l1_data_cache_policy', type=str,
                    default=c.DEFAULT_L1_DATA_CACHE_REPLACEMENT_POLICY,
                    help='Data cache type')
parser.add_argument('--l1_data_cache_miss_cycles', type=int,
                    default=c.DEFAULT_L1_DATA_CACHE_MISS_CYCLES,
                    help='Number of cycles spent in the event of a cache miss')
parser.add_argument('--l1_data_cache_assoc', type=int,
                    default=c.DEFAULT_L1_DATA_CACHE_ASSOCIATIVITY,
                    help='L1 data cache associativity')
parser.add_argument('--cpu_mhz', type=float, default=1000.0,
                    help='Speed of cpu in mhz. (Default 1GHz)')
parser.add_argument('--nic_speed_mbps', type=float,
                    default=c.DEFAULT_NIC_SPEED_MBPS,
                    help='Nic speed mbps')
parser.add_argument('--rob_size', type=int,
                    default=c.DEFAULT_ROB_SIZE,
                    help='Re-order buffer size of processor')
parser.add_argument('--dispatch_units', type=int,
                    default=c.DEFAULT_NUM_DISPATCH_UNITS,
                    help='Number of instruction dispatch units of processor')
parser.add_argument('--timing_model', type=str,
                    default=c.DEFAULT_TIMING_MODEL,
                    help='Timing model used: NONE or EMPIRICAL or ANALYTICAL')

parser.add_argument(
    '--init', help='Initializes ttn. This is run automatically during '
                   'installation. It needs to be normally run only once.',
    action='store_true')
parser.add_argument('--add', help='Adds the project to db', action='store_true')
parser.add_argument('--activate', help='Activates the project specified by name',
                    action='store_true')
parser.add_argument('--deactivate', help='Deactivates the currently active '
                    'project', action='store_true')
parser.add_argument('--reset', help='re-initializes the project clang params',
                    action='store_true')
parser.add_argument('--delete', help='Deletes the project from internal db',
                    action='store_true')
parser.add_argument('--list', help='Lists all tracked projects',
                    action='store_true')
parser.add_argument('--show', help='Displays details of the selected project. '
                    'If no project argument is specified, displays current '
                    'project.', action='store_true')
parser.add_argument('--extract', help='Extracts lookahead for selected project',
                    action='store_true')

def main():
    args = parser.parse_args()
    params = {
        c.PROJECT_ARCH_NAME: "NONE" if args.arch == "ARCH_NONE" else args.arch,
        c.PROJECT_NAME_KEY: args.project_name,
        c.PROJECT_SRC_DIR_KEY: args.project_src_dir,

        c.L1_INS_CACHE_SIZE_KEY: args.l1_ins_cache_size_kb,
        c.L1_INS_CACHE_LINES_SIZE_KEY: args.l1_ins_cache_lines,
        c.L1_INS_CACHE_REPLACEMENT_POLICY_KEY: args.l1_ins_cache_policy,
        c.L1_INS_CACHE_MISS_CYCLES_KEY: args.l1_ins_cache_miss_cycles,
        c.L1_INS_CACHE_ASSOCIATIVITY_KEY: args.l1_ins_cache_assoc,

        c.L1_DATA_CACHE_SIZE_KEY: args.l1_data_cache_size_kb,
        c.L1_DATA_CACHE_LINES_SIZE_KEY: args.l1_data_cache_lines,
        c.L1_DATA_CACHE_REPLACEMENT_POLICY_KEY: args.l1_data_cache_policy,
        c.L1_DATA_CACHE_MISS_CYCLES_KEY: args.l1_data_cache_miss_cycles,
        c.L1_DATA_CACHE_ASSOCIATIVITY_KEY: args.l1_data_cache_assoc,

        c.TIMING_MODEL_KEY: args.timing_model,
        c.ROB_SIZE_KEY: args.rob_size,
        c.DISPATCH_UNITS_KEY: args.dispatch_units,

        c.NIC_SPEED_MBPS_KEY: args.nic_speed_mbps,
        c.CPU_CYCLE_NS_KEY: float(args.cpu_mhz * 1e6)/float(1e9)
    }
    if args.list:
        ops.listAllProjects()
        sys.exit(0)

    if args.show:
        if args.project_name == c.DEFAULT_PROJECT_NAME:
            ops.displayProject(None, print_active=True)
        else:
            ops.displayProject(args.project_name, print_active=False)
        sys.exit(0)

    if args.add:

        if (not os.path.exists(ops.getInsnTimingsPath(args.arch)) and
                args.arch != c.NO_ARCH):
            print(f'Cannot initialize project with unsupported '
                  f'architecture: {args.arch}')
            print('To see list of currently supported architectures run cmd: '
                  'vtins -l')
            sys.exit(0)
        ops.addProject(args.project_name, args.project_src_dir, params)
        sys.exit(0)

    if args.activate:
        ops.activateProject(args.project_name)
        sys.exit(0)

    if args.deactivate:
        ops.deactivateProject(args.project_name)
        sys.exit(0)

    if args.reset:
        ops.resetProject(args.project_name)
        sys.exit(0)

    if args.delete:
        ops.deleteProject(args.project_name)
        sys.exit(0)

    if args.init:
        ops.initTTN()
        sys.exit(0)

    if args.extract:
        ops.handleLookaheadExtraction(args.project_name)
        sys.exit(0)

    print('No recognized command specified. Specify exactly one of: '
          '--activate/--deactivate/--reset/--delete/--init/--list/--show/'
          '--extract_la')
    sys.exit(0)

if __name__ == "__main__":
    main()
