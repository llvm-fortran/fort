# -*- Python -*-

import os
import platform

import lit.formats

# Configuration file for the 'lit' test runner.

# name: The name of this test suite.
config.name = 'Fort'

# testFormat: The test format to use to interpret tests.
#
# For now we require '&&' between commands, until they get globally killed and
# the test runner updated.
execute_external = platform.system() != 'Windows'
config.test_format = lit.formats.ShTest(execute_external)

# suffixes: A list of file extensions to treat as test files.
config.suffixes = ['.f95', '.f', '.ll']

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

# test_exec_root: The root path where tests should be run.
fort_obj_root = getattr(config, 'fort_obj_root', None)
if fort_obj_root is not None:
    config.test_exec_root = os.path.join(fort_obj_root, 'test')
    config.fort_bin_dir = fort_bin_dir = os.path.join(fort_obj_root, 'bin')

# Set llvm_{src,obj}_root for use by others.
config.llvm_src_root = getattr(config, 'llvm_src_root', None)
config.llvm_obj_root = getattr(config, 'llvm_obj_root', None)

# Tweak the PATH to include the tools dir and the scripts dir.
if fort_obj_root is not None:
    llvm_tools_dir = getattr(config, 'llvm_tools_dir', None)
    if not llvm_tools_dir:
        lit.fatal('No LLVM tools dir set!')
    # Set PATH - prepend fort bin dir and LLVM tools dir
    # Fort bin dir needs to come first in case it is an out of tree build
    path = os.path.pathsep.join((fort_bin_dir, llvm_tools_dir, config.environment['PATH']))
    config.environment['PATH'] = path

    llvm_libs_dir = getattr(config, 'llvm_libs_dir', None)
    if not llvm_libs_dir:
        lit.fatal('No LLVM libs dir set!')
    path = os.path.pathsep.join((llvm_libs_dir,
                                 config.environment.get('LD_LIBRARY_PATH','')))
    config.environment['LD_LIBRARY_PATH'] = path

###

# Check that the object root is known.
if config.test_exec_root is None:
    # Otherwise, we haven't loaded the site specific configuration (the user is
    # probably trying to run on a test file directly, and either the site
    # configuration hasn't been created by the build system, or we are in an
    # out-of-tree build situation).

    # Check for 'fort_site_config' user parameter, and use that if available.
    site_cfg = lit.params.get('fort_site_config', None)
    if site_cfg and os.path.exists(site_cfg):
        lit.load_config(config, site_cfg)
        raise SystemExit

    # Try to detect the situation where we are using an out-of-tree build by
    # looking for 'llvm-config'.
    #
    # FIXME: I debated (i.e., wrote and threw away) adding logic to
    # automagically generate the lit.site.cfg if we are in some kind of fresh
    # build situation. This means knowing how to invoke the build system though,
    # and I decided it was too much magic. We should solve this by just having
    # the .cfg files generated during the configuration step.

    llvm_config = lit.util.which('llvm-config', config.environment['PATH'])
    if not llvm_config:
        lit.fatal('No site specific configuration available!')

    # Get the source and object roots.
    llvm_src_root = lit.util.capture(['llvm-config', '--src-root']).strip()
    llvm_obj_root = lit.util.capture(['llvm-config', '--obj-root']).strip()
    fort_src_root = os.path.join(llvm_src_root, "tools", "fort")
    fort_obj_root = os.path.join(llvm_obj_root, "tools", "fort")

    # Validate that we got a tree which points to here, using the standard
    # tools/fort layout.
    this_src_root = os.path.dirname(config.test_source_root)
    if os.path.realpath(fort_src_root) != os.path.realpath(this_src_root):
        lit.fatal('No site specific configuration available!')

    # Check that the site specific configuration exists.
    site_cfg = os.path.join(fort_obj_root, 'test', 'lit.site.cfg')
    if not os.path.exists(site_cfg):
        lit.fatal('No site specific configuration available! You may need to '
                  'run "make test" in your Fort build directory.')

    # Okay, that worked. Notify the user of the automagic, and reconfigure.
    lit.note('using out-of-tree build at %r' % fort_obj_root)
    lit.load_config(config, site_cfg)
    raise SystemExit

###

# Discover the 'fort' to use.

import os

def inferFort(PATH):
    # Determine which fort to use.
    fort = os.getenv('FORT')

    # If the user set fort in the environment, definitely use that and don't
    # try to validate.
    if fort:
        return fort

    # Otherwise look in the path.
    fort = lit.util.which('fort', PATH)

    if not fort:
        lit.fatal("couldn't find 'fort' program, try setting "
                  "FORT in your environment")

    return fort

# When running under valgrind, we mangle '-vg' onto the end of the triple so we
# can check it with XFAIL and XTARGET.
if lit_config.useValgrind:
    config.target_triple += '-vg'

config.fort = inferFort(config.environment['PATH'])
if not lit_config.quiet:
    lit_config.note('using fort: %r' % config.fort)
config.substitutions.append( ('%fort', ' ' + config.fort + ' ') )
config.substitutions.append( ('%test_dir', config.test_dir) )
config.substitutions.append( ('%test_debuginfo', ' ' + config.llvm_src_root + '/utils/test_debuginfo.pl ') )

# FIXME: Find nicer way to prohibit this.
config.substitutions.append(
    (' fort ', """*** Do not use 'fort' in tests, use '%fort'. ***""") )

###

# Set available features we allow tests to conditionalize on.
if platform.system() != 'Windows':
    config.available_features.add('crash-recovery')