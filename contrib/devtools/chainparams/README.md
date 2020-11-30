# Chainparams Constants

Utilities to generate chainparams constants that are compiled into the client
(see [src/chainparamsconstants.h](/src/chainparamsconstants.h).

The chainparams constants are fetched from bitcoind, dumped to intermediate
files, and then compiled into [src/chainparamsconstants.h](/src/chainparamsconstants.h).
If you're running bitcoind locally, the following instructions will work
out-of-the-box:

## Mainnet
```
bitcoind
python3 make_chainparams.py > chainparams_main.txt
```

## Testnet3
```
bitcoind --testnet
python3 make_chainparams.py -a 127.0.0.1:18332 > chainparams_test.txt
```

## Testnet4
```
bitcoind --testnet4
python3 make_chainparams.py -a 127.0.0.1:28332 > chainparams_testnet4.txt
```

## Scalenet
```
bitcoind --scalenet
python3 make_chainparams.py -a 127.0.0.1:38332 > chainparams_scalenet.txt
```

## Build C++ Header File
```
python3 generate_chainparams_constants.py . > ../../../src/chainparamsconstants.h
```

## Testing

Updating these constants should be reviewed carefully, with a
reindex-chainstate, checkpoints=0, and assumevalid=0 to catch any defect that
causes rejection of blocks in the past history.
