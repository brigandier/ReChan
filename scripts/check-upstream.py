#!/usr/bin/env python3
"""Inspect upstream main before integration without changing the working tree."""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def git(root, *args):
    result = subprocess.run(["git", "-C", str(root), *args], capture_output=True,
                            text=True, encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    return result.stdout.rstrip("\n")


def inspect(root, base="HEAD"):
    base_sha = git(root, "rev-parse", "--verify", base + "^{commit}")
    upstream = git(root, "rev-parse", "--verify", "refs/remotes/upstream/main^{commit}")
    ancestor = git(root, "merge-base", base_sha, upstream)
    counts = git(root, "rev-list", "--left-right", "--count", base_sha + "..." + upstream).split()
    changes = git(root, "diff", "--name-only", ancestor, upstream).splitlines()
    commits = git(root, "log", "--format=%H%x09%cs%x09%s", base_sha + ".." + upstream).splitlines()
    preview = git(root, "merge-tree", ancestor, base_sha, upstream)
    conflicts = []
    path = None
    for line in preview.splitlines():
        if line.startswith(("  base ", "  our ", "  their ")):
            path = line.split(maxsplit=3)[-1]
        if line.startswith("+<<<<<<< ") and path not in conflicts:
            conflicts.append(path)
    return {
        "base": base_sha, "upstream_main": upstream, "common_ancestor": ancestor,
        "local_only_commits": int(counts[0]), "upstream_only_commits": int(counts[1]),
        "commits": [dict(zip(("sha", "date", "subject"), row.split("\t", 2))) for row in commits],
        "upstream_files": changes, "text_conflicts": conflicts,
        "working_tree": git(root, "status", "--porcelain"),
        "merge_in_progress": Path(git(root, "rev-parse", "--path-format=absolute", "--git-path", "MERGE_HEAD")).exists(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="HEAD", help="local revision to compare (default: HEAD)")
    parser.add_argument("--offline", action="store_true", help="use cached upstream/main; freshness is unverified")
    parser.add_argument("--json", action="store_true", help="print structured results")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        remote = git(root, "remote", "get-url", "upstream")
        expected = {"https://github.com/SilverwireGames/ReChan", "git@github.com:SilverwireGames/ReChan",
                    "ssh://git@github.com/SilverwireGames/ReChan"}
        if remote.removesuffix(".git").rstrip("/") not in expected:
            raise RuntimeError("upstream does not point to SilverwireGames/ReChan")
        if not args.offline:
            git(root, "fetch", "--no-tags", "upstream", "+refs/heads/main:refs/remotes/upstream/main")
        report = inspect(root, args.base)
        report["upstream_refreshed"] = not args.offline
        if args.json:
            print(json.dumps(report, indent=2))
        else:
            print("Official main:", report["upstream_main"])
            print("Reference:", report["base"])
            print("Fresh remote check:" if not args.offline else "Cached reference:", not args.offline)
            print("Upstream commits absent from reference:", report["upstream_only_commits"])
            print("Text conflicts:", ", ".join(report["text_conflicts"]) or "none")
            if report["working_tree"] or report["merge_in_progress"]:
                print("Pending local changes or merge: preserve and review before integration.")
            print("Text compatibility does not establish runtime compatibility.")
        return 0
    except (RuntimeError, OSError) as error:
        print("Upstream check failed:", error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
