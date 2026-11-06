"""Compatibility entry point for the private documentation publisher."""
from pathlib import Path
import runpy
import sys
work = Path(__file__).resolve().parents[1] / '.CyDocBuild'
sys.path.insert(0, str(work))
if __name__ == '__main__': runpy.run_path(str(work / 'publish.py'), run_name='__main__')
