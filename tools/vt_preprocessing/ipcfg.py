"""Loop lookahead and BBL lookahead computation utils."""

import re
import pprint
import networkx as nx
import pathlib
import tools.vt_preprocessing.constants as c
import tools.vt_preprocessing.call_graph as cg
import vt_functions
import numpy as np
import logging
import json
import os
from typing import Dict, Any
from collections import OrderedDict

logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.WARNING)

class LoopCFG(object):
    """Represents the control flow graph of a Loop."""
    def __init__(self, loop_number: int, loop_cfg_json: Dict[str, Any],
                 ipcfg: object):
        self._loop_number = loop_number
        self._loop_cfg_json = loop_cfg_json
        self._ipcfg = ipcfg

    def GetLoopLookahead(self):
        """Returns the shortest iteration in a loop."""
        called_functions = self._loop_cfg_json.get(c.LOOP_CALLED_FNS_JSON_KEY)
        for called_fn in called_functions:
            if (self._ipcfg.interprocedural_call_graph
                    .IsPktSendReachableFromFunction(called_fn)):
                return 0
        loop_latches = [c.BBL_ENTRY_NODE.format(bbl_number=x) for x in
                        self._loop_cfg_json.get(c.LOOP_LATCHES_JSON_KEY)]
        loop_headers = [c.BBL_ENTRY_NODE.format(bbl_number=x) for x in
                        self._loop_cfg_json.get(c.LOOP_HEADERS_JSON_KEY)]
        min_shortest_path = c.MAX_BBL_LOOKAHEAD_NS
        for loop_header in loop_headers:
            for loop_latch in loop_latches:
                length = self._ipcfg.GetBBLWeight(loop_latch)
                assert length
                if loop_header != loop_latch:
                    try:
                        length += self._ipcfg.GetShortestPathLength(
                            loop_header, loop_latch)
                    except nx.NetworkXNoPath:
                        continue
                min_shortest_path = min(length, min_shortest_path)
        return min_shortest_path



class InterProceduralCFG(object):
    """Represents an inter-procedural control flow graph."""
    def __init__(self, source_storage_dir: str):
        self._source_storage_dir = source_storage_dir
        self._ipcfg = nx.DiGraph()
        self._interprocedural_call_graph = cg.InterProceduralCallGraph()
        self._function_cfgs = {}
        self._loop_cfgs = OrderedDict()
        self._bbl_lookahead = {}
        self._all_bbl_entry_nodes = []
        self._packet_send_bbl_entry_nodes = []
        self._initialization_complete = False
        self._ipcfg_computed = False

    @property
    def interprocedural_call_graph(self):
        """Returns the ipcfg object."""
        return self._interprocedural_call_graph

    def Initialize(self):
        """Computes an interprocedural call graph."""
        assert not self._initialization_complete
        for path in pathlib.Path(self._source_storage_dir).rglob(
                '*.{}'.format(c.CLANG_ARTIFACTS_EXTENSION)):
            logging.info('Parsing artifacts file: %s', path.name)
            with open ('{}/{}'.format(self._source_storage_dir,
                                      path.name)) as f:
                artifacts_content = json.load(f)
                for function_name in artifacts_content:
                    logging.info('Updating interprocedural call graph with '
                                 'function %s', function_name)
                    self._interprocedural_call_graph.UpdateCallGraph(
                        function_name=function_name,
                        function_cfg_json=artifacts_content.get(function_name))
                    loops = artifacts_content.get(function_name).get(
                        c.FN_LOOPS_JSON_KEY)
                    for loop in loops:
                        loop_number = int(loop)
                        loop_cfg_json = loops.get(loop)
                        assert loop_number not in self._loop_cfgs
                        self._loop_cfgs[loop_number] = LoopCFG(loop_number,
                                                               loop_cfg_json,
                                                               self)
        self._interprocedural_call_graph.ComposeFullCallGraph()
        logging.info('Initialization complete. Full interprocedural call graph '
                     'composed ...')
        self._initialization_complete = True

    def _ComposeInterProceduralCFG(self):
        """For computing an inter-procedural control flow graph."""
        assert self._initialization_complete

        if self._ipcfg_computed:
            return

        for function_cfg in self._interprocedural_call_graph.function_cfgs:
            function_cfg.InitializeCFG()
            self._ipcfg = nx.compose(self._ipcfg, function_cfg.fn_cfg_graph)
            if function_cfg.fn_name not in self._function_cfgs:
                self._function_cfgs[function_cfg.fn_name] = []
            self._function_cfgs[function_cfg.fn_name].append(function_cfg)

            for bbl_entry_node in function_cfg.bbl_entry_nodes:
                self._all_bbl_entry_nodes.append(bbl_entry_node)
                assert bbl_entry_node not in self._bbl_lookahead
                self._bbl_lookahead[bbl_entry_node] = c.MAX_BBL_LOOKAHEAD_NS
            for pkt_send_bbl_entry_node in function_cfg.packet_send_entry_nodes:
                self._packet_send_bbl_entry_nodes.append(
                    pkt_send_bbl_entry_node)
                assert pkt_send_bbl_entry_node in self._bbl_lookahead
                self._bbl_lookahead[pkt_send_bbl_entry_node] = 0

        for function_cfg in self._interprocedural_call_graph.function_cfgs:
            for call_site in function_cfg.call_sites:
                called_fn, call_site_entry_node, call_site_return_node =\
                    call_site
                assert called_fn in self._function_cfgs
                for function_variant_cfg in self._function_cfgs[called_fn]:
                    self._ipcfg.add_edge(call_site_entry_node,
                                         function_variant_cfg.GetFnEntryNode(),
                                         weight=0)
                    self._ipcfg.add_edge(function_variant_cfg.GetFnExitNode(),
                                         call_site_return_node, weight=0)
        if (c.MAIN_FUNCTION_NAME in self._function_cfgs and
                len(self._function_cfgs[c.MAIN_FUNCTION_NAME]) > 1):
            logging.warning('Multiple binaries detected inside: %s. For best '
                            'performance, consider separating the source folder '
                            'into multiple folders containing relevant files '
                            'for each binary', self._source_storage_dir)
        elif c.MAIN_FUNCTION_NAME not in self._function_cfgs:
            logging.warning('No binary detected inside: %s',
                            self._source_storage_dir)
        self._ipcfg_computed = True

    def GetBBLWeight(self, bbl_node):
        """Returns the weight of a basic block."""
        return self._interprocedural_call_graph.GetBBLWeight(bbl_node)

    def GetShortestPathLength(self, node_1, node_2):
        """Returns the shortest path between two nodes in the inter-procedural control flow graph."""
        return nx.shortest_path_length(self._ipcfg, node_1, node_2, 'weight')

    def ComputeLookahead(self):
        """Computes a basic block lookahead map for all bbls."""
        self._ComposeInterProceduralCFG()
        logging.info('Reversing computed Interprocedural CFG ...')
        ipcfg_reverse = self._ipcfg.reverse(copy=True)
        logging.info('Computing shortest paths from each packet send site ...')
        for pkt_send_bbl_entry_node in self._packet_send_bbl_entry_nodes:
            length = nx.single_source_shortest_path_length(
                ipcfg_reverse, pkt_send_bbl_entry_node)
            for reachable_node in length:
                if (not re.match('__BBL_([0-9]+)_ENTRY', reachable_node) or
                        length[reachable_node] < 0):
                    continue

                self._bbl_lookahead[reachable_node] = \
                    min(self._bbl_lookahead[reachable_node],
                        length[reachable_node])
        logging.info('Lookahead computation completed for %d basic blocks ',
                     len(self._bbl_lookahead))

    def DumpBBLLookahead(self, output_file: str):
        """Writes the basic block lookahead information to STDOUT or to a Json file."""
        logging.info('Dumping BBL lookahead information ...')
        bbl_lookahead_json = {}
        bbl_numbers = [int(re.match('__BBL_([0-9]+)_ENTRY', x).group(1))
                       for x in self._bbl_lookahead]
        min_bbl_number = min(bbl_numbers)
        max_bbl_number = max(bbl_numbers)

        bbl_lookahead_dump = np.array([0]*(max_bbl_number - min_bbl_number + 1),
            dtype="long")
        for bbl_entry_node in self._bbl_lookahead:
            match = re.match('__BBL_([0-9]+)_ENTRY', bbl_entry_node)
            assert match
            bbl_number = int(match.group(1))
            bbl_lookahead_dump[bbl_number - min_bbl_number] = \
                self._bbl_lookahead[bbl_entry_node]
            bbl_lookahead_json[bbl_number] = self._bbl_lookahead[bbl_entry_node]

        if not output_file:
            logging.info('No output directory specified. Dumping to STDOUT ...')
            for i in range(min_bbl_number, max_bbl_number + 1, 1):
                print(f'BBL-{i}: {bbl_lookahead_dump[i - min_bbl_number]}')
        else:
            if os.path.exists(output_file):
                os.remove(output_file)
            logging.info('Dumping BBL lookahead information to: %s ...',
                         output_file)
            _ = vt_functions.DumpLookahead(
                bbl_lookahead_dump, output_file, min_bbl_number,
                max_bbl_number - min_bbl_number + 1)
        logging.info('BBL Lookahead information dumped ...')

    def DumpLoopLookahead(self, output_file: str):
        """Dumps lookahead information for each loop to STDOUT or to a Json file."""
        logging.info('Dumping Loop lookahead information ...')

        if len(self._loop_cfgs.keys()) == 0:
            return
        min_loop_number = min(self._loop_cfgs.keys())
        max_loop_number = max(self._loop_cfgs.keys())
        loop_lookahead_json = {}
        loop_lookahead_dump = np.array([0]*(max_loop_number - min_loop_number + 1),
            dtype="long")
        for i in range(min_loop_number, max_loop_number + 1):
            if i not in self._loop_cfgs:
                continue
            loop_lookahead_dump[i - min_loop_number] = \
                self._loop_cfgs[i].GetLoopLookahead()
            loop_lookahead_json[i] = self._loop_cfgs[i].GetLoopLookahead()
        if not output_file:
            logging.info('No output directory specified. Dumping to STDOUT ...')
            for i in range(min_loop_number, max_loop_number + 1, 1):
                print(f'Loop-{i}: {loop_lookahead_dump[i - min_loop_number]}')
        else:
            if os.path.exists(output_file):
                os.remove(output_file)
            logging.info('Dumping Loop lookahead information to: %s ...',
                         output_file)
            _ = vt_functions.DumpLookahead(
                loop_lookahead_dump, output_file, min_loop_number,
                max_loop_number - min_loop_number + 1)
        logging.info('Loop lookahead information dumped ...')
