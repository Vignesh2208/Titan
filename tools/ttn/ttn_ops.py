import json
import os
import tools.ttn.ttn_constants as c
import logging
from typing import Dict

logging.basicConfig(format='%(message)s', level=logging.NOTSET)


def createOrResetTTNProject(project_name: str, project_src_dir: str,
                            params: Dict[str, str], reset=False) -> None:
    """Creates or resets a new TTN project"""
    if not os.path.exists(project_src_dir):
        raise FileNotFoundError(f'Project source directory {project_src_dir} '
                                'does not exist !')
    if not os.path.exists(f'{project_src_dir}/{c.TTN_FOLDER_NAME}'):
        os.mkdir(f'{project_src_dir}/{c.TTN_FOLDER_NAME}')

    project_clang_init_params = {
        c.BBL_COUNTER_KEY: 0,
        c.LOOP_COUNTER_KEY: 0
    }

    if not reset:
        project_params = {
            c.PROJECT_NAME_KEY: project_name,
            c.PROJECT_ARCH_NAME: params[c.PROJECT_ARCH_NAME],
            c.PROJECT_ARCH_TIMINGS_PATH_KEY: getInsnTimingsPath(
                params[c.PROJECT_ARCH_NAME]),
            c.INS_CACHE_SIZE_KEY: params[c.INS_CACHE_SIZE_KEY],
            c.INS_CACHE_LINES_KEY: params[c.INS_CACHE_LINES_KEY],
            c.INS_CACHE_TYPE_KEY: params[c.INS_CACHE_TYPE_KEY],
            c.INS_CACHE_MISS_CYCLES_KEY: params[c.INS_CACHE_MISS_CYCLES_KEY],
            c.DATA_CACHE_SIZE_KEY: params[c.DATA_CACHE_SIZE_KEY],
            c.DATA_CACHE_LINES_KEY: params[c.DATA_CACHE_LINES_KEY],
            c.DATA_CACHE_TYPE_KEY: params[c.DATA_CACHE_TYPE_KEY],
            c.DATA_CACHE_MISS_CYCLES_KEY: params[c.DATA_CACHE_MISS_CYCLES_KEY],
            c.NIC_SPEED_MBPS_KEY: params[c.NIC_SPEED_MBPS_KEY],
            c.CPU_CYCLE_NS_KEY: params[c.CPU_CYCLE_NS_KEY]
        }

    if (reset or
            not os.path.exists(
                f'{project_src_dir}/{c.TTN_FOLDER_NAME}/clang_init_params.json')):
        with open(
                f'{project_src_dir}/{c.TTN_FOLDER_NAME}/clang_init_params.json',
                'w') as f:
            json.dump(project_clang_init_params, f)

    if not reset:
        with open(f'{project_src_dir}/{c.TTN_FOLDER_NAME}/project_params.json',
                  'w') as f:
            json.dump(project_params, f)

    # Remove any locks
    if os.path.exists(f'{project_src_dir}/{c.TTN_FOLDER_NAME}/clang_flock'):
        os.remove(f'{project_src_dir}/{c.TTN_FOLDER_NAME}/clang_flock')


def addProject(project_name: str, project_src_dir: str,
               params: Dict[str, str]) -> None:
    """Creates a ttn project if it does not exist and marks it as active."""
    currDB = {'active': None, 'projects': {}}
    if os.path.exists(c.TTN_CONFIG_DIR):
        with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'r') as f:
            currDB = json.load(f)
    else:
        os.mkdir(c.TTN_CONFIG_DIR)
    project_params = {
        c.PROJECT_NAME_KEY: project_name,
        c.PROJECT_SRC_DIR_KEY: project_src_dir,
        c.PROJECT_CLANG_INIT_PARAMS_KEY: f'{project_src_dir}/clang_init_params.json',
        c.PROJECT_CLANG_LOCK_KEY: f'{project_src_dir}/clang_lock',
        c.PROJECT_ARCH_NAME: params[c.PROJECT_ARCH_NAME],
        c.PROJECT_ARCH_TIMINGS_PATH_KEY: getInsnTimingsPath(params[c.PROJECT_ARCH_NAME]),
        c.INS_CACHE_SIZE_KEY: params[c.INS_CACHE_SIZE_KEY],
        c.INS_CACHE_LINES_KEY: params[c.INS_CACHE_LINES_KEY],
        c.INS_CACHE_TYPE_KEY: params[c.INS_CACHE_TYPE_KEY],
        c.INS_CACHE_MISS_CYCLES_KEY: params[c.INS_CACHE_MISS_CYCLES_KEY],
        c.DATA_CACHE_SIZE_KEY: params[c.DATA_CACHE_SIZE_KEY],
        c.DATA_CACHE_LINES_KEY: params[c.DATA_CACHE_LINES_KEY],
        c.DATA_CACHE_TYPE_KEY: params[c.DATA_CACHE_TYPE_KEY],
        c.DATA_CACHE_MISS_CYCLES_KEY: params[c.DATA_CACHE_MISS_CYCLES_KEY],
        c.NIC_SPEED_MBPS_KEY: params[c.NIC_SPEED_MBPS_KEY],
        c.CPU_CYCLE_NS_KEY: params[c.CPU_CYCLE_NS_KEY]
    }
    currDB['active'] = project_params.copy()
    currDB['projects'][project_name] = project_params.copy()

    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'w') as f:
        json.dump(currDB, f)

    createOrResetTTNProject(project_name, project_src_dir, params)

def getInsnTimingsPath(arch_name: str) -> str:
    """Returns a file path containing instruction timings for specified architecture."""
    if arch_name == c.NO_ARCH:
        return c.NO_ARCH
    return f'{os.path.expanduser("~")}/.ttn/instructions/{arch_name}/timings.json'

def getDefaultProjectParams() -> Dict[str, str]:
    """Get project parameters for a default configuration."""
    project_params = {
        c.PROJECT_NAME_KEY: c.DEFAULT_PROJECT_NAME,
        c.PROJECT_SRC_DIR_KEY: c.DEFAULT_PROJECT_SRC_DIR,
        c.PROJECT_CLANG_INIT_PARAMS_KEY: f'{c.DEFAULT_PROJECT_SRC_DIR}/clang_init_params.json',
        c.PROJECT_CLANG_LOCK_KEY: f'{c.DEFAULT_PROJECT_SRC_DIR}/clang_lock',
        c.PROJECT_ARCH_NAME: c.DEFAULT_PROJECT_ARCH,
        c.PROJECT_ARCH_TIMINGS_PATH_KEY: getInsnTimingsPath(
            c.DEFAULT_PROJECT_ARCH),
        c.INS_CACHE_SIZE_KEY: c.DEFAULT_INS_CACHE_SIZE_KB,
        c.INS_CACHE_LINES_KEY: c.DEFAULT_INS_CACHE_LINES,
        c.INS_CACHE_TYPE_KEY: c.DEFAULT_INS_CACHE_TYPE,
        c.INS_CACHE_MISS_CYCLES_KEY: c.DEFAULT_INS_CACHE_MISS_CYCLES,
        c.DATA_CACHE_SIZE_KEY: c.DEFAULT_DATA_CACHE_SIZE_KB,
        c.DATA_CACHE_LINES_KEY: c.DEFAULT_DATA_CACHE_LINES,
        c.DATA_CACHE_TYPE_KEY: c.DEFAULT_DATA_CACHE_TYPE,
        c.NIC_SPEED_MBPS_KEY: c.DEFAULT_NIC_SPEED_MBPS,
        c.DATA_CACHE_MISS_CYCLES_KEY: c.DEFAULT_DATA_CACHE_MISS_CYCLES,
        c.CPU_CYCLE_NS_KEY: 1.0
    }
    return project_params

def initTTN() -> None:
    """Initializes ttn. This op is performed by the installation script."""
    currDB = {'active': None, 'projects': {}}
    if os.path.exists(f'{c.TTN_CONFIG_DIR}/projects.db'):
        with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'r') as f:
            currDB = json.load(f)
    else:
        if not os.path.exists(c.TTN_CONFIG_DIR):
            os.mkdir(c.TTN_CONFIG_DIR)
    currDB['active'] = getDefaultProjectParams()

    if c.DEFAULT_PROJECT_NAME not in currDB['projects']:
        currDB['projects'][c.DEFAULT_PROJECT_NAME] = getDefaultProjectParams()

    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'w') as f:
        json.dump(currDB, f)

def deactivateProject(project_name: str) -> None:
    if not os.path.exists(f'{c.TTN_CONFIG_DIR}/projects.db'):
        logging.info(f'Project {project_name} is not active !')
        return

    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'r') as f:
        currDB = json.load(f)
        if currDB['active'][c.PROJECT_NAME_KEY] != project_name:
            logging.info(f'Project {project_name} is not active !')
            return
    currDB['active'] = getDefaultProjectParams()
    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'w') as f:
        json.dump(currDB, f)
    logging.info(f'Project {project_name} deactivated')

def activateProject(project_name: str) -> None:
    if not os.path.exists(f'{c.TTN_CONFIG_DIR}/projects.db'):
        logging.info(f'Project {project_name} is not active !')
        return

    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'r') as f:
        currDB = json.load(f)
        if currDB['active'][c.PROJECT_NAME_KEY] == project_name:
            return

    if project_name not in currDB['projects']:
        print (f"Project {project_name} does not exist !")

    currDB['active'] = currDB['projects'][project_name].copy()

    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'w') as f:
        json.dump(currDB, f)
    logging.info(f'Project {project_name} activated')

def resetProject(project_name: str) -> None:
    """Resets some of the clang specific files created for a project."""
    if not os.path.exists(f'{c.TTN_CONFIG_DIR}/projects.db'):
        logging.info(f'Project {project_name} is not initialized !')
        return

    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'r') as f:
        currDB = json.load(f)
        if project_name not in currDB['projects']:
            logging.info(f'Project {project_name} is not initialized !')
            return
    project_params = currDB['projects'][project_name]
    project_src_dir = project_params[c.PROJECT_SRC_DIR_KEY]
    createOrResetTTNProject(project_name=project_name,
                            project_src_dir=project_src_dir,
                            params=None,
                            reset=True)

def deleteProject(project_name: str) -> None:
    """Removes ttn book-keeping associated with a project."""
    if not os.path.exists(f'{c.TTN_CONFIG_DIR}/projects.db'):
        logging.info(f'Project {project_name} is not initialized !')
        return

    if project_name == c.DEFAULT_PROJECT_NAME:
        logging.info('DEFAULT project cannot be deleted !')
        return

    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'r') as f:
        currDB = json.load(f)
        if project_name not in currDB['projects']:
            logging.info(f'Project {project_name} is not initialized !')
            return
    project_params = currDB['projects'][project_name]
    project_src_dir = project_params[c.PROJECT_SRC_DIR_KEY]
    
    # remove ttn folder inside project source directory
    if os.path.exists(f'{project_src_dir}/{c.TTN_FOLDER_NAME}'):
        import shutil
        shutil.rmtree(f'{project_src_dir}/{c.TTN_FOLDER_NAME}')

    if currDB['active'][c.PROJECT_NAME_KEY] == project_name:
        currDB['active'] = getDefaultProjectParams()
        logging.info(f'Removed {project_name} from db. '
                     f'Current active project is: DEFAULT')


    del currDB['projects'][project_name]

    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'w') as f:
        json.dump(currDB, f)
    

def listAllProjects() -> None:
    """Lists all recognized project names."""
    if not os.path.exists(f'{c.TTN_CONFIG_DIR}/projects.db'):
        logging.info('ttn not initialized ! Run ttn init before trying '
                     'this command')
        return

    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'r') as f:
        currDB = json.load(f)
    for project_name in sorted(currDB['projects'].keys()):
        if project_name == currDB['active'][c.PROJECT_NAME_KEY]:
            print(f'{project_name} *')
        else:
            print(project_name)


def displayProject(project_name: str, print_active=False) -> None:
    """Display parameters for a specific chosen project."""
    if not os.path.exists(f'{c.TTN_CONFIG_DIR}/projects.db'):
        logging.info('ttn not initialized ! Run ttn init before trying '
                     'this command')
        return

    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'r') as f:
        currDB = json.load(f)

    if print_active:
        project_params = currDB['active']
    elif project_name in currDB['projects']:
        project_params = currDB['projects'][project_name]
    else:
        logging.info(f'{project_name} not found. Please run ttn list to get '
                     'the list of tracked projects')
        return

    logging.info('General params:')
    logging.info(
        f'project name             : {project_params[c.PROJECT_NAME_KEY]}')
    logging.info(
        f'project src directory    : {project_params[c.PROJECT_SRC_DIR_KEY]}')
    logging.info(
        f'project architecture     : {project_params[c.PROJECT_ARCH_NAME]}')
    logging.info(
        f'ins cache size kb        : {project_params[c.INS_CACHE_SIZE_KEY]}')
    logging.info(
        f'ins cache lines          : {project_params[c.INS_CACHE_LINES_KEY]}')
    logging.info(
        f'ins cache miss cycles    : {project_params[c.INS_CACHE_MISS_CYCLES_KEY]}')
    logging.info(
        f'ins cache type           : {project_params[c.INS_CACHE_TYPE_KEY]}')
    logging.info(
        f'data cache size kb       : {project_params[c.DATA_CACHE_SIZE_KEY]}')
    logging.info(
        f'data cache lines         : {project_params[c.DATA_CACHE_LINES_KEY]}')
    logging.info(
        f'data cache miss cycles   : {project_params[c.DATA_CACHE_MISS_CYCLES_KEY]}')
    logging.info(
        f'data cache type          : {project_params[c.DATA_CACHE_TYPE_KEY]}')
    logging.info(
        f'cpu cycle per ns         : {project_params[c.CPU_CYCLE_NS_KEY]}')
    logging.info(
        f'nic speed mbps           : {project_params[c.NIC_SPEED_MBPS_KEY]}')

    project_src_dir = project_params[c.PROJECT_SRC_DIR_KEY]
    if not os.path.exists(
            f'{project_src_dir}/{c.TTN_FOLDER_NAME}/clang_init_params.json'):
        bbl_counter = 0
        loop_counter = 0
    else:
        with open(
                f'{project_src_dir}/{c.TTN_FOLDER_NAME}/clang_init_params.json',
                'r') as f:
            value = json.load(f)
            bbl_counter = value[c.BBL_COUNTER_KEY]
            loop_counter = value[c.LOOP_COUNTER_KEY]
    logging.info('')
    logging.info('Clang specific params:')
    logging.info(f'bbl counter              : {bbl_counter}')
    logging.info(f'loop counter             : {loop_counter}')

def handleLookaheadExtraction(project_name: str) -> None:
    """Invokes vtpp script to extract and store lookahead information associated with project."""

    if not os.path.exists(f'{c.TTN_CONFIG_DIR}/projects.db'):
        logging.info('ttn not initialized ! Run ttn init before trying '
                     'this command')
        return

    with open(f'{c.TTN_CONFIG_DIR}/projects.db', 'r') as f:
        currDB = json.load(f)

    if project_name not in currDB['projects']:
        logging.info(f'No recognized project with name: {project_name} found !')
        return

    project_src_dir = currDB['projects'][project_name][c.PROJECT_SRC_DIR_KEY]
    lookahead_output_dir = f'{project_src_dir}/.ttn/lookahead'
    import subprocess

    vtpp_path = f'{os.path.expanduser("~")}/Titan/tools/vt_preprocessing/vtpp.sh'
    _ = subprocess.run([vtpp_path, '--source_dir', project_src_dir,
                        '--output_dir', lookahead_output_dir],
                        stdout=subprocess.PIPE, env=os.environ.copy())
    logging.info('Lookahead extraction complete.')