# TestGen

Utilities to generate test vectors for the data-driven Bitcoin tests.

Usage:

```
gen_base58_test_vectors.py valid 50 > ../../src/test/data/base58_keys_valid.json
gen_base58_test_vectors.py invalid 50 > ../../src/test/data/base58_keys_invalid.json
```

## ASERT test vectors

To build test vectors for the difficulty adjustment algorithm, run:

```
cmake -GNinja .. -DBUILD_ASERT_TEST_VECTORS=ON
ninja asert-testgen
```
