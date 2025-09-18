# tinyrack

Modular synthesizer emulator.

https://github.com/user-attachments/assets/3409c658-f9ec-434b-8252-e1e96a088131

## Building

Prerequisites:
* Windows
* python3
* Clang (clang + wasm-ld) - https://clang.llvm.org/get_started.html
* shader_minifier - https://github.com/laurentlb/shader-minifier
* WABT (wasm2wat + wasm-objdump + wasm-strip) - https://github.com/WebAssembly/wabt
* minify - https://github.com/tdewolff/minify

Run the make.bat script to build the project. 
```
> make.bat
```

Use any web server to serve the contents of the `bin` directory.
```
bin/font.webp
bin/index.html
bin/rack.wasm
```

The provided python script works well during development.
```
> python serve.py
```
