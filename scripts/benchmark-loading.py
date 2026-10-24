"""Measure flushed game-state logs using an isolated data/configuration directory.

Use the same runtime directory, settings and extracted assets for both executables.
This measures process startup and scripted initial level entry, not manual menu dwell,
disc extraction, cold-cache storage performance or every transition in gameplay.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import subprocess
import time


def measure(exe, runtime, level, frames, timeout, flow=None, score_percent=None):
    log = runtime / 'rechan.log'
    if log.exists():
        log.unlink()
    start = time.perf_counter()
    command = [str(exe), '--autotest', str(level), '--frames', str(frames), '--fps', '60']
    if flow:
        command += ['--benchmark-flow', flow]
    if score_percent is not None:
        command += ['--benchmark-score-percent', str(score_percent)]
    process = subprocess.Popen(command,
                               cwd=runtime, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    events, offset, partial, flow_seconds = [], 0, '', None
    while True:
        now = time.perf_counter()
        if log.exists():
            with log.open('r', encoding='utf-8', errors='replace') as stream:
                stream.seek(offset)
                chunk = stream.read()
                offset = stream.tell()
            lines = (partial + chunk).split('\n')
            partial = lines.pop()
            for line in lines:
                match = re.search(r'\[Game\] State: (\d+) -> (\d+)', line)
                if match:
                    events.append({'from': int(match[1]), 'to': int(match[2]), 'seconds': now-start})
                timing = re.search(r'\[Benchmark\] \w+ ready seconds=([\d.]+)', line)
                if timing:
                    flow_seconds = float(timing[1])
        if process.poll() is not None:
            break
        if now-start > timeout:
            process.kill()
            process.wait()
            raise RuntimeError(f'timeout after {timeout}s: {exe.name}, level {level}')
        time.sleep(0.001)
    if process.returncode:
        raise RuntimeError(f'{exe.name} exit {process.returncode}; inspect {log}')
    first = lambda state: next((e['seconds'] for e in events if e['to'] == state), None)
    title, queued, preplay, play = (first(state) for state in (3,20,7,8))
    if None in (title,queued,preplay,play):
        raise RuntimeError(f'incomplete state trace: {events}')
    if flow and flow_seconds is None:
        raise RuntimeError('executable did not report the requested benchmark flow')
    return dict(startup_to_title=title, queue_to_preplay=preplay-queued,
                queue_to_play=play-queued, startup_to_play=play,
                process_total=time.perf_counter()-start, events=events, flow_seconds=flow_seconds)


def summarize(samples):
    result = {}
    metrics = ['startup_to_title','queue_to_preplay','queue_to_play','startup_to_play','process_total']
    if samples[0]['flow_seconds'] is not None:
        metrics.append('flow_seconds')
    for metric in metrics:
        values = [s[metric] for s in samples]
        result[metric] = {'n': len(values), 'median': statistics.median(values), 'min': min(values), 'max': max(values)}
        if len(values) >= 20:
            result[metric]['p95'] = sorted(values)[math.ceil(len(values)*0.95)-1]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--a', type=Path, required=True)
    parser.add_argument('--b', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--levels', type=int, nargs='+', default=[0,1,2])
    parser.add_argument('--runs', type=int, default=5)
    parser.add_argument('--frames', type=int, default=2)
    parser.add_argument('--timeout', type=float, default=120)
    parser.add_argument('--flow', choices=['finish', 'restart', 'hub', 'petal'])
    parser.add_argument('--score-percent', type=int, choices=range(101), metavar='0..100')
    args = parser.parse_args()
    if args.runs < 1 or args.frames < 1 or args.timeout <= 0:
        parser.error('runs, frames and timeout must be positive')
    runtime = args.runtime.resolve()
    # A private runtime prevents autosaves/settings/logs from touching the installation.
    if not (runtime / '.benchmark-runtime').is_file():
        parser.error('runtime must contain .benchmark-runtime and isolated userfiles')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    config = (runtime/'userfiles/game.ini').read_bytes()
    seeds = {p.name: p.read_bytes() for p in (runtime/'userfiles').glob('*.sav')}
    records = {'method': 'alternating A/B, warm OS cache, 1ms flushed-log observation; first pair is warmup',
               'scenario': {'levels': args.levels, 'runs': args.runs, 'frames': args.frames,
                            'flow': args.flow, 'score_percent': args.score_percent},
               'settings_sha256': hashlib.sha256(config).hexdigest(),
               'executables': {name: {'path': str(exe.resolve()), 'sha256': hashlib.sha256(exe.read_bytes()).hexdigest()}
                               for name,exe in [('A',args.a),('B',args.b)]}, 'samples': []}
    for level in args.levels:
        for run in range(args.runs+1):
            for label in (('A','B') if run % 2 == 0 else ('B','A')):
                (runtime/'userfiles/game.ini').write_bytes(config)
                for name, contents in seeds.items():
                    (runtime/'userfiles'/name).write_bytes(contents)
                exe = args.a if label == 'A' else args.b
                sample = measure(exe.resolve(), runtime, level, args.frames, args.timeout, args.flow, args.score_percent)
                sample.update(label=label, level=level, run=run, warmup=run == 0)
                records['samples'].append(sample)
                args.output.write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8')
                print(f'{label} level={level} run={run}: title={sample["startup_to_title"]:.3f}s load={sample["queue_to_play"]:.3f}s',flush=True)
    records['summary'] = {str(level): {label: summarize([s for s in records['samples'] if s['level']==level and s['label']==label and not s['warmup']])
                                      for label in ('A','B')} for level in args.levels}
    args.output.write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8')


if __name__ == '__main__':
    main()
