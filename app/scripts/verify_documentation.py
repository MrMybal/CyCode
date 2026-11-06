"""Compatibility entry point; documentation tooling lives in app/.CyDocBuild."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / '.CyDocBuild'))
from verify import verify

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(); parser.add_argument('site', type=Path)
    verify(parser.parse_args().site)
