"""Dev launcher: sets up sys.path, enables mock I2C, starts Flask."""
import os
import sys

root = os.path.dirname(os.path.abspath(__file__))
rpi  = os.path.join(root, 'rpi')

os.chdir(rpi)
sys.path.insert(0, rpi)
os.environ.setdefault('MIATA_ENV', 'mock')

import runpy
runpy.run_module('src.main', run_name='__main__')
