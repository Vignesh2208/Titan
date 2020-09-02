"""Computing local CFGs, call graphs of each function and global call graphs."""

import networkx as nx
import tools.vt_preprocessing.constants as c
from typing import Dict, Any
import logging

logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.NOTSET)

class FunctionCFG(object):
    """Represents the control flow graph of a resolvable function."""
    def __init__(self, fn_name: str, variant_number: int,
                 global_call_graph: object,
                 function_cfg_json: Dict[str, str]):
        self._fn_name = fn_name
        self._fully_qualified_fn_name = \
            '{fn_name}__variant_{variant_number}'.format(
                fn_name=fn_name, variant_number=variant_number)
        self._called_functions = set()
        self._fn_cfg = nx.DiGraph()
        self._packet_send_call_sites = []
        self._bbl_entry_nodes = []
        self._packet_send_entry_nodes = []
        self._call_sites_by_fn = {}
        self._global_call_graph = global_call_graph
        self._function_cfg_json = function_cfg_json

    @property
    def fn_name(self):
        """Returns function name."""
        return self._fn_name

    @property
    def fully_qualified_fn_name(self):
        """Retruns fully qualified function name i.e function name + variant number."""
        return self._fully_qualified_fn_name

    @property
    def packet_send_call_sites(self):
        """Returns all call sites where packet send events happen."""
        return self._packet_send_call_sites

    @property
    def called_functions(self):
        """Returns all functions called from this function."""
        return self._called_functions

    @property
    def fn_cfg_graph(self):
        """Returns the function's CFG."""
        return self._fn_cfg

    @property
    def global_call_graph(self):
        """Returns the global call graph."""
        return self._global_call_graph

    @property
    def call_sites_by_fn(self):
        """Returns all call sites grouped by function they call."""
        return self._call_sites_by_fn

    @property
    def call_sites(self):
        """Yields an iterator to all call sites."""
        all_call_sites = []
        for called_fn in self._call_sites_by_fn:
            all_call_sites.extend(self._call_sites_by_fn[called_fn])
        for call_site in all_call_sites:
            yield call_site

    @property
    def bbl_entry_nodes(self):
        """Yields an iterator to entry nodes to the function."""
        for bbl_entry in self._bbl_entry_nodes:
            yield bbl_entry

    @property
    def packet_send_entry_nodes(self):
        """Yields an iterator to basic blocks where packet send calls are made."""
        for bbl_entry in self._packet_send_entry_nodes:
            yield bbl_entry

    def InitializeCallGraph(self):
        """Computes a local call graph."""
        bbls = self._function_cfg_json.get(c.FN_BBLS_JSON_KEY)
        for bbl in bbls:
            for called_fn in bbls[bbl].get(c.BBL_CALLED_FNS_JSON_KEY):
                self._called_functions.add(called_fn)
                if called_fn in c.PKT_SEND_CALL_SITES:
                    self._packet_send_call_sites.append(bbl)
                    self._packet_send_entry_nodes.append(
                        c.BBL_ENTRY_NODE.format(bbl_number=bbl))
        logging.info('Called functions: %s', self._called_functions)

    def GetFnEntryNode(self):
        """Returns the basic block at which the function is entered."""
        return c.FN_ENTRY_NODE.format(func_name=self._fully_qualified_fn_name)

    def GetFnExitNode(self):
        """Returns the hypothetical node at which the function is exited."""
        return c.FN_EXIT_NODE.format(func_name=self._fully_qualified_fn_name)

    def InitializeCFG(self):
        """Computes local CFG for the function."""
        assert self._global_call_graph.is_composed
        fn_entry_node = c.FN_ENTRY_NODE.format(
            func_name=self._fully_qualified_fn_name)
        fn_exit_node = c.FN_EXIT_NODE.format(
            func_name=self._fully_qualified_fn_name)
        fn_entry_bbl = self._function_cfg_json.get(c.FN_ENTRY_BBL_JSON_KEY)
        fn_return_bbls = self._function_cfg_json.get(c.FN_RETURN_BBLS_JSON_KEY)
        self._fn_cfg.add_node(fn_entry_node)
        self._fn_cfg.add_node(fn_exit_node)

        bbls = self._function_cfg_json.get(c.FN_BBLS_JSON_KEY)
        for bbl in bbls:
            curr_node = c.BBL_ENTRY_NODE.format(bbl_number=bbl)
            bbl_weight = bbls[bbl].get(c.BBL_WEIGHT_JSON_KEY)
            self._global_call_graph.SetBBLWeight(curr_node, bbl_weight)
            self._fn_cfg.add_node(curr_node)
            self._bbl_entry_nodes.append(curr_node)

            if bbl == fn_entry_bbl:
                self._fn_cfg.add_edge(fn_entry_node, fn_entry_bbl, weight=0)

            call_site_number = 0
            for called_fn in bbls[bbl].get(c.BBL_CALLED_FNS_JSON_KEY):
                if not self._global_call_graph.IsFunctionResolvable(called_fn):
                    continue
                if called_fn not in self._call_sites_by_fn:
                    self._call_sites_by_fn[called_fn] = []
                call_site_entry_node = c.BBL_CALLSITE_ENTRY_NODE.format(
                    bbl_number=bbl, call_site_number=call_site_number)
                call_site_return_node = c.BBL_CALLSITE_RETURN_NODE.format(
                    bbl_number=bbl, call_site_number=call_site_number)
                self._call_sites_by_fn[called_fn].append(
                    (called_fn, call_site_entry_node, call_site_return_node))
                self._fn_cfg.add_edge(curr_node, call_site_entry_node, weight=0)
                curr_node = call_site_return_node
                call_site_number += 1
                self._fn_cfg.add_node(curr_node)
            if call_site_number > 0:
                self._fn_cfg.add_edge(curr_node,
                                      c.BBL_EXIT_NODE.format(bbl_number=bbl),
                                      weight=0)
                curr_node = c.BBL_EXIT_NODE.format(bbl_number=bbl)
            for out_bbl in bbls[bbl].get(c.BBL_OUT_EDGES_JSON_KEY):
                self._fn_cfg.add_edge(curr_node,
                                      c.BBL_ENTRY_NODE.format(
                                          bbl_number=out_bbl),
                                      weight=bbl_weight)
            if bbl in fn_return_bbls:
                self._fn_cfg.add_edge(curr_node, fn_exit_node, weight=0)


class InterProceduralCallGraph(object):
    """Represents an inter-procedural call graph computed by merging local callgraphs from all functions."""
    def __init__(self):
        self._call_graph = nx.DiGraph()
        self._resolvable_functions = {}
        self._function_reachability = {}
        self._packet_send_sites = set()
        self._bbl_weights = dict()
        self._is_composed = False

    @property
    def resolvable_functions(self):
        """A function is resolvable if the body is defined in the source i.e it is not in some external library."""
        return self._resolvable_functions

    @property
    def function_reachability(self):
        """Returns a dictionary which represents functions reachable from a given function."""
        return self._function_reachability

    @property
    def packet_sent_sites(self):
        """Returns all call sites where packets are sent."""
        return self._packet_send_sites

    def SetBBLWeight(self, bbl_node, weight):
        """Sets the weight of the basic block to represent time consumed in executing the basic block."""
        self._bbl_weights[bbl_node] = weight

    def GetBBLWeight(self, bbl_node):
        """Returns the weight of a basic block."""
        return self._bbl_weights.get(bbl_node)

    @property
    def is_composed(self):
        return self._is_composed

    @property
    def function_cfgs(self):
        """Yields a local cfg for each resolvable funciton."""
        for function_name in self._resolvable_functions:
            for function_cfg in self._resolvable_functions[function_name]:
                yield function_cfg

    def IsFunctionResolvable(self, fn_name: str) -> bool:
        """Returns true if a given function name is resolvable."""
        return True if fn_name in self._resolvable_functions else False

    def IsPktSendReachableFromFunction(self, fn_name: str) -> bool:
        """Returns true if a packet send call site is reachable from a function."""
        if (not self.IsFunctionResolvable(fn_name) and
                not fn_name in c.PKT_SEND_CALL_SITES):
            return False
        if fn_name in c.PKT_SEND_CALL_SITES:
            return True
        return self._function_reachability[fn_name]

    def UpdateCallGraph(self, function_name: str,
                        function_cfg_json: Dict[str, Any]):
        """Merging local callgraphs of a resolvable function into a global callgraph."""
        logging.info('Function cfg json: %s', function_cfg_json)
        if function_name not in self._resolvable_functions:
            self._resolvable_functions[function_name] = []
            self._function_reachability[function_name] = False
        variant_number = len(self._resolvable_functions[function_name])
        function_cfg = FunctionCFG(fn_name=function_name,
                                   variant_number=variant_number,
                                   global_call_graph=self,
                                   function_cfg_json=function_cfg_json)
        function_cfg.InitializeCallGraph()
        self._resolvable_functions[function_name].append(function_cfg)
        if function_cfg.packet_send_call_sites:
            self._function_reachability[function_name] = True
            self._packet_send_sites.update(
                function_cfg.packet_send_call_sites)

    def ComposeFullCallGraph(self):
        """Compute the final global call graph across all resolvable functions."""
        if self._is_composed:
            return

        packet_send_nodes = set()
        created_nodes = set()
        for resolvable_function in self._resolvable_functions:
            if resolvable_function not in created_nodes:
                self._call_graph.add_node(resolvable_function)
            called_functions_union = set()
            for function_cfg in \
                    self._resolvable_functions[resolvable_function]:
                called_functions_union.update(
                    function_cfg.called_functions)
            for called_function in called_functions_union:
                self._call_graph.add_edge(called_function,
                                          resolvable_function)
                created_nodes.add(resolvable_function)
                created_nodes.add(called_function)
                if called_function in c.PKT_SEND_CALL_SITES:
                    packet_send_nodes.add(called_function)
        for packet_send_node in packet_send_nodes:
            length = nx.single_source_shortest_path_length(
                self._call_graph, packet_send_node)
            for reachable_node in length:
                if length[reachable_node] >= 0:
                    self._function_reachability[reachable_node] = True
        logging.info('Function reachability: %s', self._function_reachability)
        self._is_composed = True
