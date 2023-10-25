#!/usr/bin/env python3
#
# Derived from run-clang-tidy.py and adapted for Code Browser
# See https://llvm.org/LICENSE.txt for license information of
# the original file
#
import argparse
import json
import multiprocessing
import os
import shutil
import subprocess
import sys
import threading
from pathlib import Path
import queue as queue
from collections import OrderedDict
import time

suffix = "___suf"


def make_absolute(f, directory):
    if os.path.isabs(f):
        return f
    return os.path.normpath(os.path.join(directory, f))


def generate(args, queue, lock, tid):
    while True:
        name = queue.get()
        cmd = [args.gen, "-b", args.compile_commands, "-o", args.out_dir]
        for project in args.projects:
            cmd.append("-p")
            cmd.append(project)
        if args.externalprojects is not None:
            for p in args.externalprojects:
                cmd.append("-e")
                cmd.append(p)

        cmd.append(name)

        my_env = os.environ.copy()
        my_env["MULTIPROCESS_MODE"] = suffix + str(tid)
        proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=my_env)
        output, err = proc.communicate()
        if proc.returncode != 0:
            if proc.returncode < 0:
                msg = "%s: terminated by signal %d\n" % (
                    name, -proc.returncode)
                err += msg.encode("utf-8")
        with lock:
            sys.stdout.write(" ".join(cmd) + "\n" + output.decode("utf-8"))
            if len(err) > 0:
                sys.stdout.flush()
                sys.stderr.write(err.decode("utf-8"))
        queue.task_done()


def do_file(directory, f, max_task):
    # for a file name f
    # look for all f___suf[0 - max_task]
    # then merge them into one file and delete them
    # leaving only the merged file behind
    possible_files = list()
    for i in range(max_task):
        filepath = directory.joinpath(f + suffix + str(i))
        if filepath.exists():
            possible_files.append(filepath)

    txtlines = OrderedDict()
    for fil in possible_files:
        lines = str(fil.read_text()).splitlines()
        for line in lines:
            txtlines[line] = None

    txt = '\n'.join(txtlines.keys())
    new_file = directory.joinpath(f)
    # print("create new file: {}".format(str(new_file)))
    new_file.write_text(txt)
    # remove old ones
    for fil in possible_files:
        fil.unlink()


def do_merge_dir(d, max_task):
    dirPath = Path(d)
    files = set()
    for f in os.listdir(d):
        fn = os.fsdecode(f)
        try:
            if fn.index(suffix) != -1:
                files.add(fn.split(suffix)[0])
        except ValueError:
            continue

    for f in files:
        do_file(dirPath, f, max_task)


def do_merge(out, max_task):
    fnsearch = out + "/fnSearch"
    print("Merging ", fnsearch)
    do_merge_dir(fnsearch, max_task)

    refs = out + "/refs"
    print("Merging ", refs)
    do_merge_dir(refs, max_task)

    refsM = out + "/refs/_M"
    print("Merging ", refsM)
    do_merge_dir(refsM, max_task)

    print("Merging fileIndex")
    do_merge_dir(out, max_task)


def main():
    usage = "python runner.py -p compile_commands.json -o output/ -e ./generator/codebrowser_generator -a project_name -x external_project"
    parser = argparse.ArgumentParser(
        description="Runs codebrowser over all files in a compile_commands.json in parallel.", usage=usage)
    parser.add_argument("-j", type=int, default=0,
                        help="number of generators to be run in parallel.")
    parser.add_argument(
        "-e", dest="gen", help="Path to codebrowser_generator.")
    parser.add_argument("-p", dest="compile_commands",
                        help="Path to a compile_commands.json file.")
    parser.add_argument("-o", dest="out_dir",
                        help="Path to output directory.")
    parser.add_argument("-a", dest="projects", action='extend', nargs='*',
                        help="List of project names, source directory and version, specify as NAME:SOURCE_DIR:VERSION")
    parser.add_argument("-x", dest="externalprojects", action='extend', nargs='*',
                        help="List of external project names, source directory and version, specify as project:path:url")

    args = parser.parse_args()

    compile_commands = args.compile_commands
    if compile_commands is None:
        print("Error: No compile commands specified")
        exit(1)

    if args.gen is None:
        print("Error: Please specify path to codebrowser_generator")
        exit(1)

    if args.projects is None:
        print("Error: Please specify at least one project")
        exit(1)

    if args.out_dir is None:
        print("Error: No output specified")
        exit(1)

    compile_commands = os.path.abspath(compile_commands)
    if not os.path.exists(compile_commands):
        print("Error: invalid path to compile_commands".format(compile_commands))
        exit(1)

    print("using compile_commands.json: {}".format(compile_commands))

    # Load the database and extract all files.
    database = json.load(open(compile_commands))
    files = set(
        [make_absolute(entry["file"], entry["directory"])
         for entry in database]
    )

    max_task = args.j
    if max_task == 0:
        max_task = multiprocessing.cpu_count()

    try:
        task_queue = queue.Queue(max_task)
        # List of files with a non-zero return code.
        lock = threading.Lock()
        idx = 0
        for _ in range(max_task):
            t = threading.Thread(
                target=generate, args=(args, task_queue, lock, idx))
            t.daemon = True
            t.start()
            idx = idx + 1

        # Fill the queue with files.
        for name in files:
            task_queue.put(name)

        # Wait for all threads to be done.
        task_queue.join()

    except KeyboardInterrupt:
        print("\nCtrl-C detected, goodbye.")
        if tmpdir:
            shutil.rmtree(tmpdir)
        os.kill(0, 9)

    print("Merging files...")
    start = time.time()

    do_merge(args.out_dir, max_task)

    end = time.time()
    print("Merged all files in: %.2F seconds" % (end - start))

    sys.exit()


if __name__ == "__main__":
    main()

# kate: indent-width 4; replace-tabs on;
