#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
Converts CLI help text to Markdown.

Instead of executing this script directly, run gen-manpages.sh, which will in turn invoke this script.
'''

import sys

# Create list of lines in CLI help text
lines = sys.argv[1].split('\n')

# Helper functions for generating (up-to-)two-column Markdown tables


def createTable(first, second):
    global table, heads, widths
    table = []
    heads = (first, second)
    widths = [len(head) for head in heads]


def appendTableRow(first, second):
    widths[0] = max(widths[0], len(first))
    widths[1] = max(widths[1], len(second))
    table.append((first, second))


def printTable(columns=2):
    line = '|'
    for col in range(columns):
        line += ' ' + heads[col].ljust(widths[col]) + ' |'
    print(line)
    line = '|'
    for col in range(columns):
        # Use a colon to force left alignment
        line += ' :' + '-' * (widths[col] - 1) + ' |'
    print(line)
    for row in table:
        line = '|'
        for col in range(columns):
            line += ' ' + row[col].ljust(widths[col]) + ' |'
        print(line)


# Print document title (binary name and version)
print('# ' + lines[0])
print('')

i = 2

# Print usage table
createTable('Usage', 'Description')
columns = 1
while lines[i][:8] in ('Usage:  ', 'or:     '):
    row = (lines[i][8:] + '  ').split('  ', 1)
    description = row[1].strip()
    appendTableRow('`' + row[0] + '`', description)
    if description:
        columns = 2
    i += 1
printTable(columns)

i += 1

# Loop over option categories until end of help text
while i < len(lines):
    # Print category title
    print('')
    category = lines[i][:-1]
    print(category)
    print('-' * len(category))
    print('')
    # Print options table for this category
    createTable('Argument', 'Description')
    i += 2
    while i < len(lines) and lines[i][:2] == '  ':
        # Put the option in one code span per alias
        option = '`' + lines[i][2:].replace(', ', '`, `') + '`'
        description = lines[i + 1][7:]
        i += 2
        while i < len(lines) and lines[i][:7] == '       ':
            description += ' ' + lines[i][7:]
            i += 1
        # HTML tag delimiters need to be escaped in Markdown
        description = description.replace('<', '&lt;').replace('>', '&gt;')
        appendTableRow(option, description)
        i += 1
    printTable()
