"""Constants used in lookahead artifact processing."""

FN_ENTRY_BBL_JSON_KEY = 'entry_block'
FN_RETURN_BBLS_JSON_KEY = 'returning_blocks'
FN_BBLS_JSON_KEY = 'bbls'
FN_LOOPS_JSON_KEY = 'loops'
BBL_CALLED_FNS_JSON_KEY = 'called_fns'
BBL_IN_EDGES_JSON_KEY = 'in_edges'
BBL_OUT_EDGES_JSON_KEY = 'out_edges'
BBL_WEIGHT_JSON_KEY = 'weight'
LOOP_CALLED_FNS_JSON_KEY = 'loop_called_fns'
LOOP_HEADERS_JSON_KEY = 'loop_headers'
LOOP_LATCHES_JSON_KEY = 'loop_latches'

FN_ENTRY_NODE = '__FN_{func_name}_ENTRY'
FN_EXIT_NODE = '__FN_{func_name}_EXIT'
BBL_ENTRY_NODE = '__BBL_{bbl_number}_ENTRY'
BBL_EXIT_NODE = '__BBL_{bbl_number}_EXIT'
BBL_CALLSITE_ENTRY_NODE = '__BBL_{bbl_number}_CALLSITE_ENTRY_{call_site_number}_CALLED_FN_{called_fn}'
BBL_CALLSITE_RETURN_NODE = '__BBL_{bbl_number}_CALLSITE_RETURN_{call_site_number}_CALLED_FN_{called_fn}'

PKT_SEND_CALL_SITES = ['send', 'sendto', 'sendmsg', 'write', 'connect', 'read', '__unknown__']

CLANG_ARTIFACTS_EXTENSION = 'artifacts.json'
MAIN_FUNCTION_NAME = 'main'

NS_IN_US = 1000
NS_IN_MS = 1000000
MAX_BBL_LOOKAHEAD_NS = 1000*NS_IN_MS

BBL_LOOKAHEAD_FILE_NAME = 'bbl_lookahead.info'
LOOP_LOOKAHEAD_FILE_NAME = 'loop_lookahead.info'

IGNORED_CALLED_FUNCTIONS = ['insCacheCallback', 'dataWriteCacheCallback', 'dataReadCacheCallback']
