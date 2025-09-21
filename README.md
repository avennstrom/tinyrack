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

## Architecture

Each module is made up of N inputs, internal state, M outputs. The VCO (Voltage Controlled Oscillator) is the simplest example:

```c
typedef struct tr_vco
{
    float phase; // [0..2pi]

    float in_v0;
    const float* in_voct;

    tr_buf out_sin;
    tr_buf out_sqr;
    tr_buf out_saw;
} tr_vco_t;

void tr_vco_update(tr_vco_t* vco);
```

The rack defines an internal audio buffer size of 512 samples.

When a module is updated it must produce 512 samples of audio data for each output (`tr_buf`).

An input is either:
* a pointer to an output buffer of a different module - or the same module, creating a feedback loop.
* a `float` or `int` value that can be tweaked externally, by turning a knob in the UI for example.

The connections between modules form an implicit dependency graph. The application needs to traverse this graph to figure out the correct update order. This traversal is performed in `tr_resolve_module_graph`. The function traverses the graph backwards, starting at leaf nodes (speakers, oscilloscopes). The maximum distance to a leaf node is accumulated for each module. The update order is then defined by this distance in reverse.

```
                                                          \
+-----+     +-----+     +-----+     +-----------+       \  |
| VCO | --> | VCF | --> | VCA | --> |  Speaker  | --> ) |  |
+-----+     +-----+     +-----+     +-----------+       /  |
               |                                          /
               v
        +--------------+
        | Oscilloscope |
        +--------------+
        |  __         /|
        | /  \       / |
        |/    \   /\/  |
        |      \_/     |
        +--------------+
```

