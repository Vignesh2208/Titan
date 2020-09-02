#!/usr/bin/env python
"""Preprocesses a directory x86 instruction latency tables for each architecture."""

from ezodf import opendoc
from pandas_ods_reader import read_ods
from typing import List
import pandas as pd
import numpy as np
import re
import sys
import argparse
import logging
import os

logging.basicConfig(level=logging.NOTSET)

parser = argparse.ArgumentParser(
    description='Generates a json file containing '
                'avg x86 instruction latency cycles')
parser.add_argument('--arch_name', type=str, default='all',
                    help='Name of the architecture to parse')
parser.add_argument('--output_dir', type=str, default='/tmp',
                    help='Outputs are created here.')
parser.add_argument('--list', help='List all architectures with generated '
                    'instruction timings ', action='store_true')



def pruneDFrame(df: pd.DataFrame) -> pd.DataFrame:
    """Creates a dataframe containing instruction names and avg latency cycles."""

    prunedDF = {}
    processedDF = {'insn_name': [], 'avg_latency': []}
    instructionNameColIdx = 0
    latencyColIdx = None
    p = re.compile(r'\d+')

    def slashSubstitution(base_string: str) -> List[str]:
        """Substitutes slashes in base string."""

        slash_split = re.split('[/ ,]', base_string)

        if len(slash_split) <= 1:
            return slash_split
        n = len(slash_split[0])
        for idx, curr_split in enumerate(slash_split):
            if idx == 0: continue
            m = len(curr_split)
            if m > n:
                slash_split[idx] = curr_split
            else:
                slash_split[idx] = (f'{slash_split[0][0:n - m]}{curr_split}')

        return slash_split

    def insnGroupSubstitution(insn_group_name: str) -> List[str]:
        """For an instruction group possible containing braces and slashes, generates all possible group names."""
        valid_prefixes = []
        concat_stack_string = ''
        concat_with_stack_string = False
        for c in insn_group_name:
            if c == '(':
                concat_stack_string = ''
                concat_with_stack_string = True
            elif c == ')':
                slash_splits = slashSubstitution(concat_stack_string)
                concat_with_stack_string = False
                if not valid_prefixes: valid_prefixes = slash_splits
                else:
                    valid_prefixes_copy = valid_prefixes.copy()
                    valid_prefixes = []
                    for i, _ in enumerate(valid_prefixes_copy):
                        valid_prefixes.append(valid_prefixes_copy[i])
                        for j, _ in enumerate(slash_splits):
                            valid_prefixes.append(
                                f'{valid_prefixes_copy[i]}{slash_splits[j]}')
            elif ((c >= 'A' and c <= 'Z') or (c == 'c')
                  or (c == '/') or (c == ',') or (c >= '0' and c <= '9')):
                if not concat_with_stack_string and not valid_prefixes:
                    valid_prefixes.append(c)
                elif not concat_with_stack_string:
                    valid_prefixes = [f'{x}{c}' for x in valid_prefixes]
                else:
                    concat_stack_string += c

        substituted_valid_prefixes = []
        for prefix in valid_prefixes:
            substituted_valid_prefixes.extend(slashSubstitution(prefix))
        return substituted_valid_prefixes


    def mergeTwoExpandedInsnGroups(insn_group_1: List[str],
                                   insn_group_2: List[str]) -> List[str]:
        """Combines/contatenates all valid prefixes of two instruction groups."""

        merged_insn_group = set()
        for prefix_1 in insn_group_1:
            for prefix_2 in insn_group_2:
                n, m = len(prefix_1), len(prefix_2)
                if m > n:
                    merged_insn_group.add(prefix_2)
                else:
                    if prefix_1[n-m:] in [
                        'B', 'W', 'D', 'Q', 'BW', 'BD', 'BQ', 'DW', 'DQ',
                        'SB', 'SW', 'SD', 'UB', 'UW', 'UD', 'PS', 'SS']:
                        merged_insn_group.add(f'{prefix_1[0:n - m]}{prefix_2}')
                    else:
                        merged_insn_group.add(f'{prefix_1}{prefix_2}')
        return list(merged_insn_group)


    def isInsnGroupMergeable(insn_group: List[str]) -> bool:
        """Checks whether this instruction group prefixes need to be merged with another instruction group prefixes."""
        if not insn_group:
            return True
        if insn_group[0] in ['B', 'W', 'D', 'Q', 'BW', 'BD', 'BQ', 'DW', 'DQ',
                             'SB', 'SW', 'SD', 'UB', 'UW', 'UD', 'PS', 'SS']:
            return True
        return False


    def isInstruction(name: str) -> bool:
        """Approximately determines if the passed string is an instruction."""

        if not name or len(name) < 2:
            return False
        for c in name:
            if (c < 'A' or c > 'Z' ) and (c != 'c')  and (c < '0') and (c > '9'):
                return False
        return True


    def expandInsns(insn_name: str) -> str:
        """Expands read string into all possible instruction instantiations."""
        insn_groups = re.split('[\n ,]', insn_name)
        insn_group_prefixes = []
        for insn_group_name in insn_groups:
            insn_group_prefixes.append(insnGroupSubstitution(
                insn_group_name))

        base_idx = 0

        for insn_name in insn_group_prefixes[base_idx]:
            yield  insn_name

        for i, _ in enumerate(insn_group_prefixes):
            if i == 0: continue
            if isInsnGroupMergeable(insn_group_prefixes[i]):
                merged = mergeTwoExpandedInsnGroups(
                    insn_group_1=insn_group_prefixes[base_idx],
                    insn_group_2=insn_group_prefixes[i])
                for merged_insn_name in merged:
                    yield  merged_insn_name
            else:

                if base_idx:
                    for insn_name in insn_group_prefixes[base_idx]:
                        yield insn_name
                base_idx = i
                if i == len(insn_group_prefixes) - 1:
                    for insn_name in insn_group_prefixes[base_idx]:
                        yield insn_name

    for _, row in df.iterrows():
        if not latencyColIdx and 'Latency' in row.unique():
            latencyColIdx = list(row).index('Latency')
            print(f'Determined latency col idx = {latencyColIdx}')
            continue
        if latencyColIdx:
            numbers = [int(x) for x in p.findall(str(row[latencyColIdx]))]
            if not numbers:
                continue

            numbers_avg = np.mean(numbers)
            insn_name = row[instructionNameColIdx]
            for expanded_insn_name in expandInsns(insn_name):
                condition_codes_expansion = []
                if 'cc' in expanded_insn_name:
                    n = len(condition_codes_expansion)
                    x86_condition_codes = ['O', 'NO', 'B', 'NAE', 'NB', 'AE',
                                           'E', 'Z', 'NE', 'NZ', 'BE', 'NA',
                                           'NBE', 'A', 'S', 'NS', 'P', 'PE',
                                           'NP', 'PO', 'L', 'NGE', 'NL', 'GE',
                                           'LE', 'NG', 'NLE', 'G']
                    for cc in x86_condition_codes:
                        condition_codes_expansion.append(
                            expanded_insn_name.replace('cc', cc, 1))
                else:
                    condition_codes_expansion.append(expanded_insn_name)
                for cc_expanded_insn_name in condition_codes_expansion:
                    if (isInstruction(cc_expanded_insn_name) and
                            cc_expanded_insn_name not in prunedDF):
                        prunedDF[cc_expanded_insn_name] = []
                    if (isInstruction(cc_expanded_insn_name)):
                        prunedDF[cc_expanded_insn_name].append(numbers_avg)

    for expanded_insn_name in prunedDF:
        prunedDF[expanded_insn_name] = np.mean(prunedDF[expanded_insn_name])
    for insn_name, avg_latency in prunedDF.items():
        processedDF['insn_name'].append(insn_name)
        processedDF['avg_latency'].append(avg_latency)
    return pd.DataFrame.from_dict(processedDF)

def main():
    args = parser.parse_args()

    if args.list:
        print('Currently supported architectures with instruction timings: ')
        supported = [x for x in os.listdir(args.output_dir)]
        for arch in supported:
            print(arch)
        sys.exit(0)
    script_directory = os.path.dirname(os.path.realpath(__file__))
    instruction_tables_path = f'{script_directory}/instruction_tables.ods'
    doc = opendoc(instruction_tables_path)
    exceptions = ['Introduction', 'Definition of terms', 'Instruction sets',
                  'Microprocessors tested']
    all_sheets = [sheet.name for sheet in doc.sheets]
    for exception in exceptions:
        all_sheets.remove(exception)

    if (args.arch_name != 'all' and args.arch_name not in all_sheets):
        logging.error('Passed arch name not supported. It must be one of %s',
                      all_sheets)
        sys.exit(-1)

    logging.info(
        f'Generating instruction timings for architectures: {args.arch_name}')
    sheets = all_sheets if args.arch_name == 'all' else [args.arch_name]
    for sheet_name in sheets:
        # load a sheet based on its name
        logging.info(f'***** ARCH: {sheet_name} *****')
        df = read_ods(instruction_tables_path, sheet_name, headers=False)
        arch_insn_timings_df = pruneDFrame(df)
        if arch_insn_timings_df.empty:
            logging.warning(f'No valid instruction timings for arch: {sheet_name}')
            continue
        sheet_name_formatted = sheet_name.replace(' ', '_')
        logging.info(f'Number of instructions with timings: {len(arch_insn_timings_df)}')
        if not os.path.exists(f'{args.output_dir}/{sheet_name_formatted}'):
            os.makedirs(f'{args.output_dir}/{sheet_name_formatted}')
        arch_insn_timings_df.to_json(
            f'{args.output_dir}/{sheet_name_formatted}/timings.json')

    sys.exit(0)

if __name__ == "__main__":
    main()