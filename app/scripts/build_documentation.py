"""Compatibility entry point; documentation tooling lives in app/.CyDocBuild."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / '.CyDocBuild'))
from build import main, publish

if __name__ == '__main__':
    main()
