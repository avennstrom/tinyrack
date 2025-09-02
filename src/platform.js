mergeInto(LibraryManager.library, {
    js_init: function() {
        /** @type {HTMLCanvasElement} */
        let canvas = document.getElementById('canvas');
        let gl = canvas.getContext("webgl2", {antialias: true});

        this.vertex_buffer = gl.createBuffer();
        this.vao = gl.createVertexArray();
        
        // --- Shaders ---
        const vsSrc = `#version 300 es
        uniform mat4 uView;
        uniform mat4 uProjection;
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec4 aColor;
        out vec4 vColor;
        void main() {
            gl_Position = uProjection * uView * vec4(aPos, 0.0, 1.0);
            vColor = aColor;
        }
        `;

        const fsSrc = `#version 300 es
        precision mediump float;
        in vec4 vColor;
        out vec4 fragColor;
        void main() {
            fragColor = vColor;
        }
        `;

        // --- Compile/Link ---
        function compile(src, type) {
            const s = gl.createShader(type);
            gl.shaderSource(s, src);
            gl.compileShader(s);
            if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
                console.error(gl.getShaderInfoLog(s));
            }
            return s;
        }

        const prog = gl.createProgram();
        gl.attachShader(prog, compile(vsSrc, gl.VERTEX_SHADER));
        gl.attachShader(prog, compile(fsSrc, gl.FRAGMENT_SHADER));
        gl.linkProgram(prog);

        window.addEventListener('mousedown', (ev) => {
            Module._js_mousedown(ev.button);
        });
        window.addEventListener('mouseup', (ev) => {
            Module._js_mouseup(ev.button);
        });
        window.addEventListener('mousemove', (ev) => {
            Module._js_mousemove(ev.clientX, ev.clientY);
        });
        window.addEventListener('keydown', (ev) => {
            console.log('down: ' + ev.key);
        });
        window.addEventListener('keyup', (ev) => {
            console.log('up: ' + ev.key);
        });
        
        this.prog = prog;
        this.canvas = canvas;
        this.gl = gl;
    },
    js_render: function(draw_ptr, draw_count, vertex_data_ptr, vertex_count) {
        /** @type {HTMLCanvasElement} */
        const canvas = this.canvas;
        /** @type {WebGL2RenderingContext} */
        const gl = this.gl;

        const vertex_stride = 12;
        const vertex_data = new Uint8Array(wasmMemory.buffer, vertex_data_ptr, vertex_count * vertex_stride);

        gl.bindVertexArray(this.vao);

        gl.bindBuffer(gl.ARRAY_BUFFER, this.vertex_buffer);
        gl.bufferData(gl.ARRAY_BUFFER, vertex_data, gl.STREAM_DRAW);
        
        gl.enableVertexAttribArray(0);
        gl.vertexAttribPointer(0, 2, gl.FLOAT, false, vertex_stride, 0);
        gl.enableVertexAttribArray(1);
        gl.vertexAttribPointer(1, 4, gl.UNSIGNED_BYTE, true, vertex_stride, 8);

        const W = canvas.width;
        const H = canvas.height;
        const proj = new Float32Array([
             2 / W,  0,      0, 0,
             0,     -2 / H,  0, 0,
             0,      0,     -1, 0,
            -1,      1,      0, 1
        ]);

        gl.useProgram(this.prog);

        gl.viewport(0, 0, canvas.width, canvas.height);
        gl.clearColor(60 / 255, 83 / 255, 119 / 255, 1);
        gl.clear(gl.COLOR_BUFFER_BIT);

        const dv = new DataView(HEAPU8.buffer);

        const u_view = gl.getUniformLocation(this.prog, "uView");
        const u_projection = gl.getUniformLocation(this.prog, "uProjection");

        gl.uniformMatrix4fv(u_projection, false, proj);

        for (let i = 0; i < draw_count; ++i)
        {
            const draw_base_ptr = draw_ptr + i * (64+12);
            const view = new Float32Array(wasmMemory.buffer, draw_base_ptr, 16);
            gl.uniformMatrix4fv(u_view, true, view);
            const topology = dv.getInt32(draw_base_ptr + 64, true);
            const vertex_offset = dv.getUint32(draw_base_ptr + 64 + 4, true);
            const vertex_count = dv.getUint32(draw_base_ptr + 64 + 8, true);
            //console.log({topology, vertex_offset, vertex_count});
            gl.drawArrays(topology, vertex_offset, vertex_count);
        }
    },
});