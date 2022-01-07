# Markdownlint style definition - see .mdlrc
all
#exclude-tag :line_length

# Header style
#rule 'MD003', :style => 'consistent'

# Unordered list indentation
rule 'MD007', :indent => 4

# line length:
rule 'MD013', :line_length => 300

# multiple headers with the same content
#rule 'MD024', :allow_different_nesting => true
exclude_rule 'MD024'

# Trailing punctuation in header
exclude_rule 'MD026'

# ordered list item prefix
rule 'MD029', :style => 'ordered'

# fenced code blocks should have a language specified:
exclude_rule 'MD040'

# code block style:
#rule 'MD046', :style => 'consistent'
exclude_rule 'MD046'
