# exSat EVM

 exSat EVM is a compatibility layer deployed on top of the Antelope blockchain which implements the Ethereum Virtual Machine (EVM). It enables developers to deploy and run their applications on top of the Antelope blockchain infrastructure but to build, test, and debug those applications using the common languages and tools they are used to using with other EVM compatible blockchains. It also enables users of those applications to interact with the application in ways they are familiar with (e.g. using a MetaMask wallet).

The exSat EVM consists of multiple components that are tracked across different repositories.

This repository hosts the source to build the exSat EVM Contract:
exSat EVM Contract: This is the Antelope smart contract that implements the main runtime for the EVM. The source code for the smart contract can be found in the `contracts` directory. The main build artifacts are `evm_runtime.wasm` and `evm_runtime.abi`.



## Compilation

### checkout the source code:
```
git clone https://github.com/exsat-network/evm-contract.git
cd evm-contract
git submodule update --init --recursive
```


### compile EVM smart contract for Antelope blockchain:
Prerequisites:
- cmake 3.16 or later
- install cdt
```
wget https://github.com/AntelopeIO/cdt/releases/download/v3.1.0/cdt_3.1.0_amd64.deb
sudo apt install ./cdt_3.1.0_amd64.deb
```
or refer to the detail instructions from https://github.com/AntelopeIO/cdt

steps of building EVM smart contracts:
```
mkdir build
cd build
cmake ..
make -j
```
You should get the following output files:
```
evm-contract/build/evm_runtime/evm_runtime.wasm
evm-contract/build/evm_runtime/evm_runtime.abi
```

## Unit tests

We need to compile the Leap project in Antelope in order to compile unit tests:
following the instruction in https://github.com/AntelopeIO/leap to compile leap

To compile unit tests:
```
cd evm-contract/tests
mkdir build
cd build
cmake -Deosio_DIR=/<PATH_TO_LEAP_SOURCE>/build/lib/cmake/eosio ..
make -j4 unit_test
```

to run unit test:
```
cd tests/build
./unit_test
```
