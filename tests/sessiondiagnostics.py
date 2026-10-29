"""Windows integration fixtures; requires a --tests build and intentionally crashes isolated children."""
from pathlib import Path
import concurrent.futures
import importlib.util
import json
import os
import shutil
import subprocess
import tempfile
import time
import struct
import atexit
import argparse
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe', type=Path)
args = parser.parse_args()
if os.name != 'nt': raise SystemExit('Native dump integration tests require Windows')
children = []
@atexit.register
def stop_children():
    for process in children:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)

root = Path(__file__).resolve().parents[1]
work = root / 'build/tests/session-diagnostics'
work.mkdir(parents=True, exist_ok=True)
runtime = Path(tempfile.mkdtemp(prefix='ReChan-session-QA-'))
(runtime / 'pc').mkdir()
exe = args.exe.resolve() if args.exe else root / 'bin/rechan.exe'
env = dict(os.environ, RECHAN_CRASH_REPORTER_NO_DIALOG='1')
spec = importlib.util.spec_from_file_location('sessions', root / 'scripts/summarize-sessions.py')
sessions = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sessions)
results = []
def run(argument, folder=runtime, environment=env):
    result = subprocess.run([str(exe), argument], cwd=folder, env=environment, capture_output=True, timeout=20)
    lines = result.stderr.decode('utf-8', errors='replace').splitlines()
    log = Path(next(line.split('[Session] log=', 1)[1] for line in lines if '[Session] log=' in line))
    return result, log
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    runs = list(pool.map(run, ['--test-session-logging'] * 2))
assert len({log for _, log in runs}) == 2
for result, log in runs:
    assert result.returncode == 0 and log.parent == runtime / 'logs'
    assert sessions.summarize(log)['outcome'] == 'normal'
    assert 'marker persisted' in log.read_text()
results.append('PASS concurrent sessions: unique names, no overwrite, normal markers')
before = {log: log.read_bytes() for _, log in runs}
result, crash_log = run('--test-crash-reporter')
assert result.returncode != 0
report = crash_log.with_name(crash_log.stem + '-crash.txt')
dump = crash_log.with_name(crash_log.stem + '-crash.dmp')
assert report.is_file() and dump.read_bytes().startswith(b'MDMP')
assert str(crash_log) in report.read_text()
assert sessions.summarize(crash_log)['outcome'] == 'fatal-report-present'
assert all(log.read_bytes() == data for log, data in before.items())
results.append('PASS fatal signal: matching session/report/native MDMP, previous logs preserved')
exception, exception_log = run('--test-crash-exception')
assert exception.returncode != 0
exception_report = exception_log.with_name(exception_log.stem + '-crash.txt').read_text()
assert 'Exception code: 0xC0000005' in exception_report and 'Game state: 21' in exception_report
assert 'Level: 4' in exception_report and 'Last operation: diagnostic exception during region reload' in exception_report
assert 'diagnostic exception during region reload' in exception_report.split('Session log tail')[1]
exception_dump = exception_log.with_name(exception_log.stem + '-crash.dmp')
assert exception_dump.read_bytes().startswith(b'MDMP')
data = exception_dump.read_bytes()
stream_count, directory = struct.unpack_from('<II', data, 8)
streams = [struct.unpack_from('<III', data, directory + 12 * index) for index in range(stream_count)]
exception_stream = next(offset for kind, size, offset in streams if kind == 6)
assert struct.unpack_from('<I', data, exception_stream + 8)[0] == 0xc0000005
thread = int(re.search(r'Thread: (\d+)', exception_report)[1])
assert struct.unpack_from('<I', data, exception_stream)[0] == thread
assert 'Native minidump: written (Windows error 0)' in exception_report
shutil.copy2(exception_log, work / 'exception-fixture.log')
shutil.copy2(exception_log.with_name(exception_log.stem + '-crash.txt'), work / 'exception-fixture-crash.txt')
shutil.copy2(exception_dump, work / 'exception-fixture-crash.dmp')
results.append('PASS structured exception: exception code, thread/context, last operation and matching native dump')
fallback = runtime / 'readonly-simulation'
(fallback / 'pc').mkdir(parents=True)
(fallback / 'logs').write_text('blocking-file')
fallback_env = dict(env, LOCALAPPDATA=str(runtime / 'local-app-data'))
result, fallback_log = run('--test-session-logging', fallback, fallback_env)
assert result.returncode == 0 and fallback_log.parent == runtime / 'local-app-data/ReChan/logs'
results.append('PASS unwritable logs directory: LOCALAPPDATA fallback and effective path reported')
process = subprocess.Popen([str(exe), '--test-session-hold'], cwd=runtime, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
children.append(process)
deadline = time.monotonic() + 10
while time.monotonic() < deadline:
    held = [path for path in (runtime / 'logs').glob('*.log') if 'waiting for external termination' in path.read_text(errors='replace')]
    if held: break
    time.sleep(0.05)
else:
    process.kill()
    raise AssertionError('External termination fixture not ready')
process.kill(); process.communicate(timeout=10)
assert sessions.summarize(held[0])['outcome'].startswith('incomplete-or-unspecified')
results.append('PASS externally terminated fixture: incomplete session remains unknown without crash evidence')
previous_logs = set((runtime / 'logs').glob('*.log'))
active = subprocess.Popen([str(exe), '--test-session-hold'], cwd=runtime, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
children.append(active)
for _ in range(100):
    active_logs = [path for path in set((runtime / 'logs').glob('*.log')) - previous_logs
                   if 'waiting for external termination' in path.read_text(errors='replace')]
    if active_logs: break
    time.sleep(0.05)
assert active_logs
for index in range(25):
    run('--test-session-logging')
assert active.poll() is None and active_logs[0].exists()
assert len(list((runtime / 'logs').glob('*.log'))) == 21
active.kill(); active.communicate(timeout=10)
run('--test-session-logging')
assert len(list((runtime / 'logs').glob('*.log'))) == 20
assert not crash_log.exists() and not report.exists() and not dump.exists()
assert not held[0].exists()
results.append('PASS retention: newest 20 sessions retained; expired log/report/dump family removed together')
destination = work / ('diagnostic-qa-evidence-' + runtime.name)
assert not destination.exists()
shutil.copytree(runtime, destination)
(work / 'diagnostic-qa.json').write_text(json.dumps({'runtime': str(runtime), 'results': results}, indent=2), encoding='utf-8')
print('\n'.join(results))

