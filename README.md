# PolyGlot, a fuzzing framework for language processors

## Build
We tested `PolyGlot` on `Ubuntu 18.04`.

1. Get the source code: `git clone https://github.com/s3team/Polyglot && cd PolyGlot`
2. Install prerequisite: `sudo apt install -y make python g++ bison flex clang-format clang`
3. Modify the `Makefile` to choose the language you want to test
4. Build everything: `make`
5. The fuzzer is in `AFL_replate_mutate/afl-fuzz`
6. Use the `afl-gcc/afl-g++/afl-clang/afl-clang++` in `AFL_replace_mutate` to compile the program you want to fuzz.

## Config the semantic.json
Before we run the fuzzer, we need to set some values in `semantic.json`. Here are some important values that you should set:

1. `InitFileDir`: This should be an absolute path of your init seed file dir. It can be the same as/different from your path of input.
2. `BuiltinObjFile`: If you want to use the build-in functions/variables/class for semantic validation, set this path (not a single file). Refer to `grammar/solidity_grammar/semantic.json` for an example. 

## Run
To run the fuzzer, we just run it like normal `afl-fuzz`: 
```
afl-fuzz -i path/to/inputs -o path/to/outputs -- prog [args @@]
```

You should collect your own seed inputs for the fuzzer.

## Apply on a new language
To do

## Video tutorial
[![asciicast](https://asciinema.org/a/i9ohn5ncc98SlBQR4f1g33KpT.svg)](https://asciinema.org/a/i9ohn5ncc98SlBQR4f1g33KpT)

## Publication
[**One Engine to Fuzz ‘em All: Generic Language Processor Testing with Semantic Validation**](https://changochen.github.io/publication/polyglot_sp_2021_to_appear.pdf)

Yongheng Chen, Rui Zhong(co-first author), Hong Hu, Hangfan Zhang, Yupeng Yang, Dinghao Wu and Wenke Lee.</br>
In Proceedings of the 41st IEEE Symposium on Security and Privacy (Oakland 2021).

## Contact
[Yongheng Chen](https://changochen.github.io/): <ne0@gatech.edu>

Rui Zhong: <reversezr33@gmail.com>

[Hong Hu](https://huhong789.github.io/): <honghu@psu.edu>

Hangfan Zhang: <hbz5148@psu.edu>

Yupeng Yang: <yype@foxmail.com>

[Dinghao Wu](https://faculty.ist.psu.edu/wu/): <dwu@ist.psu.edu>

[Wenke Lee](https://wenke.gtisc.gatech.edu/): <wenke@cc.gatech.edu>
