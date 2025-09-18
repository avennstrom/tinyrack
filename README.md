# tinyrack

Modular synthesizer emulator.

https://github.com/user-attachments/assets/3409c658-f9ec-434b-8252-e1e96a088131

## Building

Prerequisites:
* Windows
* python3
* Clang (clang + wasm-ld) - https://clang.llvm.org/get_started.html
* shader_minifier - https://github.com/laurentlb/shader-minifier
* WABT (wasm2wat + wasm-objdump) - https://github.com/WebAssembly/wabt
* minify - https://github.com/tdewolff/minify

Run the make.bat script to build the project. 
```
$ cmd
$ make
```

TODO:
* cmake?
* VST?
* more modules
* better UX
