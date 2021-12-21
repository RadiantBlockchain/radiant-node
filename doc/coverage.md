# Generate coverage report

Coverage measurement is typically used to gauge the effectiveness of tests. It
can show which parts of your code are being exercised by tests, and which are not.

## Dependencies

The following dependencies are required for building coverage reports:

* c++filt
* gcov
* genhtml
* lcov
* python3

On Debian/Ubuntu

```
apt-get install binutils gcc lcov python3
```

## Build

Set `ENABLE_COVERAGE` when running cmake. To enable branch coverage, set
`ENABLE_BRANCH_COVERAGE`.

```
  cmake -GNinja .. \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DENABLE_COVERAGE=ON \
    -DENABLE_BRANCH_COVERAGE=ON
```

## Targets

The coverage target is associated to the test target.

`ninja coverage-check` builds a coverage report for the check target.

`ninja coverage-check-functional` for the check-functional target, etc.

To get a global coverage, one can use `ninja coverage-check-all` or
`ninja coverage-check-extended`.

The coverage report is generated in a folder named `<target>.coverage`. For
example running `ninja coverage-check-all` will generate the report in the
folder `check-all.coverage`.

## secp256k1 standalone

```
 cmake -GNinja .. \
    -DCMAKE_C_COMPILER=gcc \
    -DSECP256K1_ENABLE_COVERAGE=ON \
    -DSECP256K1_ENABLE_BRANCH_COVERAGE=ON
  ninja coverage-check-secp256k1
```
