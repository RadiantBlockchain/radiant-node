## Linting

Linting is the automated checking of our source code for programmatic and
stylistic errors.

This project has a collection of linters under `test/lint`.

To run all linters, run `ninja check-lint`. To run individual linters, run
`ninja check-lint-<name of linter>`, for example `ninja check-lint-check-doc`.
It's also possible to execute linters directly from the root directory, such as
`./test/lint/check-doc-py`.

### Adding a linter

Add a new script in `test/lint`. The working directory for the linters are the
project root directory.

Add your linter to `test/lint/CMakeLists.txt`.
