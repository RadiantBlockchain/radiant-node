#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
Converts JSON-RPC help text to Markdown.

Instead of executing this script directly, run gen-manpages.sh, which will in turn invoke this script.
'''

import sys

# Create list of lines in JSON-RPC help text
lines = sys.argv[1].split('\n')

# Print title (command name)
title = '`' + (lines[0] + ' ').split(' ', 2)[0] + '` JSON-RPC command'
print(title)
print('=' * len(title))
print('')

# Print synopsis
print('**`' + lines[0] + '`**')

i = 1

headingPermitted = False
section = []


def printSection():
    # Truncate any trailing empty lines
    while section and section[-1] == '':
        section.pop()
    # Check if there is anything to print
    if section:
        # Print the section
        print('')
        print('```')
        for line in section:
            print(line)
        print('```')


while i < len(lines):
    if headingPermitted and lines[i] and lines[i][-1] == ':':
        printSection()
        # Print the heading
        heading = lines[i][:-1]
        print('')
        print(heading)
        print('-' * len(heading))
        headingPermitted = False
        section = []
    else:
        # Truncate any leading empty lines
        if section or lines[i]:
            section.append(lines[i])
        # The next line can create a heading if this line is empty
        headingPermitted = lines[i] == ''
    i += 1
printSection()
