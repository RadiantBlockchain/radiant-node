# Fuzz-testing Radiant Node

A special test harness in `src/test/fuzz/` is provided for each fuzz target to
provide an easy entry point for fuzzers and the like. In this document we'll
describe how to use it with AFL and libFuzzer.

## Preparing fuzzing

AFL needs an input directory with examples, and an output directory where it
will place examples that it found. These can be anywhere in the file system,
we'll define environment variables to make it easy to reference them.

libFuzzer will use the input directory as output directory.

Extract the example seeds (or other starting inputs) into the inputs
directory before starting fuzzing.

```
git clone https://github.com/radiantblockchain/radn-sw/qa-assets
export DIR_FUZZ_IN=$PWD/qa-assets/fuzz_seed_corpus
```

Only for AFL:

```
mkdir outputs
export AFLOUT=$PWD/outputs
```

## AFL

### Building AFL

It is recommended to always use the latest version of afl:

```
wget http://lcamtuf.coredump.cx/afl/releases/afl-latest.tgz
tar -zxvf afl-latest.tgz
cd afl-<version>
make
export AFLPATH=$PWD
```

### Instrumentation

To build Radiant Node using AFL instrumentation (this assumes that the
`AFLPATH` was set as above):

```
mkdir -p buildFuzzer
cd buildFuzzer
cmake -GNinja .. -DCCACHE=OFF -DCMAKE_C_COMPILER=afl-gcc -DCMAKE_CXX_COMPILER=afl-g++
export AFL_HARDEN=1
ninja bitcoin-fuzzers
```

We disable ccache because we don't want to pollute the ccache with instrumented
objects, and similarly don't want to use non-instrumented cached objects linked
in.

The fuzzing can be sped up significantly (~200x) by using `afl-clang-fast` and
`afl-clang-fast++` in place of `afl-gcc` and `afl-g++` when compiling. When
compiling using `afl-clang-fast`/`afl-clang-fast++` the resulting
binary will be instrumented in such a way that the AFL features "persistent
mode" and "deferred forkserver" can be used.
See <https://github.com/google/AFL/tree/master/llvm_mode> for details.

### Fuzzing

To start the actual fuzzing use:

```
export FUZZ_TARGET=fuzz_target_foo  # Pick a fuzz_target
mkdir ${AFLOUT}/${FUZZ_TARGET}
$AFLPATH/afl-fuzz -i ${DIR_FUZZ_IN}/${FUZZ_TARGET} -o ${AFLOUT}/${FUZZ_TARGET} -m52 -- src/test/fuzz/${FUZZ_TARGET}
```

You may have to change a few kernel parameters to test optimally - `afl-fuzz`
will print an error and suggestion if so.

## libFuzzer

A recent version of `clang`, the address sanitizer and libFuzzer is needed (all
found in the `compiler-rt` runtime libraries package).

To build all fuzz targets with libFuzzer, run

```
mkdir -p buildFuzzer
cd buildFuzzer
cmake -GNinja .. \
  -DCCACHE=OFF \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_SANITIZERS="fuzzer;address"
ninja bitcoin-fuzzers
```

The fuzzer needs some inputs to work on, but the inputs or seeds can be used
interchangeably between libFuzzer and AFL.

See <https://llvm.org/docs/LibFuzzer.html#running> on how to run the libFuzzer
instrumented executable.

Alternatively run the script in `./test/fuzz/test_runner.py` and provide it
with the `${DIR_FUZZ_IN}` created earlier:
`./test/fuzz/test_runner.py -l DEBUG $DIR_FUZZ_IN`

If the `test_runner.py` script is missing, create it with
`ninja link-fuzz-test_runner.py`
