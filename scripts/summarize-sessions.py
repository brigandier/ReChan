"""Classify session evidence without equating missing shutdown markers with crashes."""
from pathlib import Path
import argparse
import json

def summarize(path):
    contents = path.read_text(encoding='utf-8', errors='replace')
    report = path.with_name(path.stem + '-crash.txt')
    dump = path.with_name(path.stem + '-crash.dmp')
    if report.exists():
        outcome = 'fatal-report-present'
    elif '[Session] end outcome=normal exit=0' in contents:
        outcome = 'normal'
    elif '[Session] end outcome=handled-error' in contents or '[Session] handled-error:' in contents:
        outcome = 'handled-error'
    else:
        outcome = 'incomplete-or-unspecified; cause unknown'
    return {'log': str(path), 'outcome': outcome, 'report': str(report) if report.exists() else None,
            'dump': str(dump) if dump.exists() else None}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    print(json.dumps([summarize(path) for path in sorted(args.directory.glob('*.log'))], indent=2, ensure_ascii=False))
