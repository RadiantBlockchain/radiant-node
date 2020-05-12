#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
Converts CLI help text files to Markdown files.

Instead of executing this script directly, run gen-manpages.sh, which will in turn invoke this script.
'''

import sys

# Create list of lines in CLI help text
lines = sys.argv[1].split('\n')

# Helper functions for generating two-column Markdown tables
def createTable(title):
    global table, heads, widths
    table = []
    # Table title is used as first column header, in bold
    # Second column header is blank (a space)
    heads = ('**' + title + '**', ' ')
    widths = [len(head) for head in heads]
def appendTableRow(first, second):
    widths[0] = max(widths[0], len(first))
    widths[1] = max(widths[1], len(second))
    table.append((first, second))
def printTable():
    print('| ' + heads[0].ljust(widths[0]) + ' | ' + ' ' * widths[1] + ' |')
    # Use a colon to force left alignment of title
    print('| :' + '-' * (widths[0] - 1) + ' | ' + '-' * widths[1] + ' |')
    for row in table:
        print('| ' + row[0].ljust(widths[0]) + ' | ' + row[1].ljust(widths[1]) + ' |')

# Print document title (binary name and version)
print('# ' + lines[0])
print('')

i = 2

# Print usage table
createTable('Usage:')
while lines[i][:8] in ('Usage:  ', 'or:     '):
    row = lines[i][8:].split('  ', 1)
    appendTableRow('`' + row[0] + '`', row[1].strip())
    i += 1
printTable()

i += 1

# Loop over option categories until end of help text
while i < len(lines):
    # Print horizontal line
    print('')
    print('***')
    print('')
    # Print options table for this category
    createTable(lines[i])
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
