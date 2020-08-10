"""Preprocesses a directory containing llvm artifacts and generates BBL + Loop lookahead maps."""

#!/usr/bin/python

import argparse
import ipcfg
import constants as c
import logging
import os

logging.basicConfig(level=logging.NOTSET)

parser = argparse.ArgumentParser(description='Generates basic block and loop '
                                             'lookahead information')
parser.add_argument('--source_dir', type=str, required=True,
                    help='Directory containing source code and binary to parse')
parser.add_argument('--output_dir', type=str, required=False,
                    help='Lookahead information is dumped to this directory',
                    default=None)



def main():
    args = parser.parse_args()

    if not os.path.isdir(args.source_dir):
        logging.error('Specified source directory: %s does not exist !',
            args.source_dir)

    logging.info('Starting virtual time lookahead computation over: %s',
                 args.source_dir)
    ipcfg_graph = ipcfg.InterProceduralCFG(args.source_dir)
    ipcfg_graph.Initialize()
    ipcfg_graph.ComputeLookahead()

    bbl_output_file = \
        '{}/{}'.format(args.output_dir, c.BBL_LOOKAHEAD_FILE_NAME) if (
            args.output_dir) else None
    loop_output_file = \
        '{}/{}'.format(args.output_dir, c.LOOP_LOOKAHEAD_FILE_NAME) if (
            args.output_dir) else None
    if args.output_dir:
        if not os.path.exists(args.output_dir):
            os.makedirs(args.output_dir)
    ipcfg_graph.DumpBBLLookahead(bbl_output_file)
    ipcfg_graph.DumpLoopLookahead(loop_output_file)



if __name__ == "__main__":
    main()