#!/usr/bin/python
# -*- encoding: utf-8 -*-

import random
import os
import sys
import subprocess


def print_help():
    print('Usage: lkh_runner.py -s LKH -o OUT_DIR -p PROBLEM -n NUMBER -m MAX_FILES')
    print("    -s   Path to the lkh software. Default 'LKH'.")
    print("    -o   Output path. All lkh input files and output tour files are generated in this directory.\n"
          "         The directory is created if it does not exists. When running multiple instances it is\n"
          "         recommended to use different output directories.")
    print("    -p   Problem file. Default: 'atsp/6.atsp'.")
    print("    -n   The problem number. Default: '6'.")
    print("    -m   Maximum number of file to keep. Default: '20'.")
    print("    -h   Display this help message.")


def getopts(argv):
    opts = {}  # Empty dictionary to store key-value pairs.
    while argv:  # While there are arguments left to parse...
        if argv[0][0] == '-':  # Found a "-name value" pair.
            if len(argv) >= 2:
                opts[argv[0]] = argv[1]  # Add key and value to the dictionary.
            else:
                opts[argv[0]] = ''
        argv = argv[1:]  # Reduce the argument list by copying it starting from index 1.
    return opts


def get_args():
    from sys import argv
    myargs = getopts(argv)

    if '-h' in myargs:
        print_help()
        sys.exit()

    if not ('-o' in myargs):
        print("Error missing mandatory -o argument.\n")
        print_help()
        sys.exit()

    directory = myargs['-o']
    if not (os.path.exists(directory)):
        os.makedirs(directory)

    lkh = myargs.get('-s', 'LKH')
    problem = myargs.get('-p', 'atsp/6.atsp')
    max_files = myargs.get('-m', 20)
    number = myargs.get('-n', 6)

    return lkh, directory, problem, int(max_files), int(number)


def generate_input_file(directory, problem, seed, number):
    print(seed)
    filename = '{}/{}.par.{}'.format(directory, number, seed)
    file = open(filename, 'w')
    file.write(
        '''
PROBLEM_FILE = {prob}
OUTPUT_TOUR_FILE = {dir}/{n}.$.lkh.{seed}
RUNS = 20
MAX_TRIALS = 3000
BACKTRACKING = YES

MAX_CANDIDATES = {n} SYMMETRIC
MOVE_TYPE = 3
PATCHING_C = 3
PATCHING_A = 2
SEED = {seed}
EOF'''.format(dir=directory, prob=problem, seed=seed, n=number)
    )
    file.close()
    return filename


def cleanup_results(directory, max_files):
    files = []
    for file in os.listdir(directory):
        files.append(file)
    files.sort()
    files.reverse()
    to_remove = files[:-max_files]
    for f in to_remove:
        os.remove(os.path.join(directory, f))


if __name__ == '__main__':

    lkh, directory, problem, max_files, number = get_args()

    while True:
        input_file = generate_input_file(directory, problem, random.randrange(1, 2147483647), number)
        subprocess.call([lkh, input_file])
        os.remove(input_file)
        cleanup_results(directory, max_files)
