mergeInto(LibraryManager.library, {
    js_set_cursor: function(cursor) {
        let canvas = document.getElementById('canvas');
        let style = 'pointer';
        switch (cursor) {
            case 0: style = 'default'; break;
            case 1: style = 'pointer'; break;
            case 2: style = 'crosshair'; break;
            case 3: style = 'arrow'; break;
            case 4: style = 'grab'; break;
            case 5: style = 'grabbing'; break;
            case 6: style = 'no-drop'; break;
            case 7: style = 'move'; break;
        }
        canvas.style.cursor = style;
    },
    js_init: function() {
        /** @type {HTMLCanvasElement} */
        let canvas = document.getElementById('canvas');
        let gl = canvas.getContext("webgl2", {antialias: true});

        canvas.style.cursor = "grab";

        const image = new Image();
        image.src = "font.png";  // your PNG path
        image.onload = () => {
            const tex = gl.createTexture();
            gl.bindTexture(gl.TEXTURE_2D, tex);
            gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true); // Flip Y so the texture coords (0,0) start at bottom-left

            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, image);

            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

            this.font_texture = tex;
        }

        this.vertex_buffer = gl.createBuffer();
        this.vao = gl.createVertexArray();
        
        const vs_color = `#version 300 es
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

        const fs_color = `#version 300 es
        precision mediump float;
        in vec4 vColor;
        out vec4 fragColor;
        void main() {
            fragColor = vColor;
        }
        `;

        const vs_font = `#version 300 es
        uniform mat4 uView;
        uniform mat4 uProjection;
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec4 aColor;
        layout(location=2) in uint aTexcoord;
        out vec4 vColor;
        out vec2 vTexcoord;
        void main() {
            gl_Position = uProjection * uView * vec4(aPos, 0.0, 1.0);
            vColor = aColor;
            uint u = aTexcoord & 0xffffu;
            uint v = aTexcoord >> 16u;
            vTexcoord = vec2(u, v);
        }
        `;

        const fs_font = `#version 300 es
        precision mediump float;
        uniform sampler2D uFont;
        in vec4 vColor;
        in vec2 vTexcoord;
        out vec4 fragColor;

        vec3 hash3(uint x) {
            x ^= x >> 16u;
            x *= 0x7feb352du;
            x ^= x >> 15u;
            x *= 0x846ca68bu;
            x ^= x >> 16u;
            return vec3(
                float((x * 0x68bc21ebu) & 0x00ffffffu),
                float((x * 0x02e5be93u) & 0x00ffffffu),
                float((x * 0x967a889bu) & 0x00ffffffu)
            ) / float(0x01000000u);
        }

        float median(float r, float g, float b) {
            return max(min(r, g), min(max(r, g), b));
        }

        void main() {
            //fragColor.rgb = hash3(uint(vTexcoord.x) | (uint(vTexcoord.y) << 16u));

            vec2 uv = (vTexcoord + 0.5) * (1.0 / 208.0);
            vec3 msdf = texture(uFont, uv).rgb;
            //fragColor.rg = uv;
            //fragColor.b = 0.0;
            //fragColor = vec4(msdf, 1);

            float sd = median(msdf.r, msdf.g, msdf.b);

            // Map signed distance to alpha (0.5 = edge)
            float w = fwidth(sd);  // screen-space smoothing
            float alpha = smoothstep(0.5 - w, 0.5 + w, sd);

            fragColor = vec4(vColor.rgb, vColor.a * alpha);
        }
        `;

        function compile(src, type) {
            const s = gl.createShader(type);
            gl.shaderSource(s, src);
            gl.compileShader(s);
            if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
                console.error(gl.getShaderInfoLog(s));
            }
            return s;
        }

        function create_program(vs_src, fs_src)
        {
            const program = gl.createProgram();
            gl.attachShader(program, compile(vs_src, gl.VERTEX_SHADER));
            gl.attachShader(program, compile(fs_src, gl.FRAGMENT_SHADER));
            gl.linkProgram(program);
            return {
                program,
                u_view: gl.getUniformLocation(program, "uView"),
                u_projection: gl.getUniformLocation(program, "uProjection"),
            };
        }

        window.addEventListener('mousedown', (ev) => {
            Module._js_mousedown(ev.button);
            this.ctx.resume();
        });
        window.addEventListener('mouseup', (ev) => {
            Module._js_mouseup(ev.button);
        });
        window.addEventListener('mousemove', (ev) => {
            Module._js_mousemove(ev.clientX, ev.clientY);
        });
        window.addEventListener('keydown', (ev) => {
            //console.log('down: ' + ev.key);
        });
        window.addEventListener('keyup', (ev) => {
            //console.log('up: ' + ev.key);
        });
        window.addEventListener('wheel', (ev) => {
            console.log('wheel: ' + ev.deltaY);
            Module._js_mousewheel(ev.deltaX, ev.deltaY);
        });
        
        this.programs = [
            create_program(vs_color, fs_color),
            create_program(vs_font, fs_font),
        ];
        this.canvas = canvas;
        this.gl = gl;
        
        (async () => {
            const ctx = new AudioContext({sampleRate: 48000});
            await ctx.audioWorklet.addModule("audio-processor.js");
            
            const audio_blocksize = 128;
            const audio_bufcount = 8; // how many 128-sample blocks to buffer
            const node = new AudioWorkletNode(ctx, "rack", {
                processorOptions: {
                    pcm_sab: new SharedArrayBuffer(4 * audio_blocksize * audio_bufcount),
                    bufcount: audio_bufcount,
                },
            });
            node.connect(ctx.destination);

            const pcm_ptr = Module._my_malloc(4 * audio_blocksize);
            const pcm = new Float32Array(wasmMemory.buffer, pcm_ptr, audio_blocksize);

            //let phase = 0.0;
            node.port.onmessage = (event) => {
                //console.log(pcm_buf);
                const pcm_buf = event.data;
                // const pd = (440.0 * Math.PI) / 48000.0;
                // for (let i = 0; i < pcm_buf.length; ++i)
                // {
                //     pcm_buf[i] = Math.sin(phase);
                //     phase += pd;
                // }
                
                instanceof.exports._tr_audio_callback(pcm_ptr, audio_blocksize);
                pcm_buf.set(pcm);
            };

            ctx.resume();
            this.ctx = ctx;
        })();
    },
    js_render: function(draw_ptr, draw_count, vertex_data_ptr, vertex_count) {
        /** @type {HTMLCanvasElement} */
        const canvas = this.canvas;
        /** @type {WebGL2RenderingContext} */
        const gl = this.gl;

        const vertex_stride = 16;
        const vertex_data = new Uint8Array(wasmMemory.buffer, vertex_data_ptr, vertex_count * vertex_stride);

        gl.bindVertexArray(this.vao);

        gl.bindBuffer(gl.ARRAY_BUFFER, this.vertex_buffer);
        gl.bufferData(gl.ARRAY_BUFFER, vertex_data, gl.STREAM_DRAW);
        
        gl.enableVertexAttribArray(0);
        gl.vertexAttribPointer(0, 2, gl.FLOAT, false, vertex_stride, 0);
        gl.enableVertexAttribArray(1);
        gl.vertexAttribPointer(1, 4, gl.UNSIGNED_BYTE, true, vertex_stride, 8);
        gl.enableVertexAttribArray(2);
        gl.vertexAttribIPointer(2, 1, gl.UNSIGNED_INT, vertex_stride, 12);

        gl.activeTexture(gl.TEXTURE0);
        if (this.font_texture !== undefined)
        {
            gl.bindTexture(gl.TEXTURE_2D, this.font_texture);
        }

        const W = canvas.width;
        const H = canvas.height;
        const proj = new Float32Array([
             2 / W,  0,      0, 0,
             0,     -2 / H,  0, 0,
             0,      0,     -1, 0,
            -1,      1,      0, 1
        ]);

        gl.viewport(0, 0, canvas.width, canvas.height);
        gl.clearColor(60 / 255, 83 / 255, 119 / 255, 1);
        gl.clear(gl.COLOR_BUFFER_BIT);

        const dv = new DataView(HEAPU8.buffer);

        let current_program = -1;

        for (let i = 0; i < draw_count; ++i)
        {
            const draw_base_ptr = draw_ptr + i * (64+16);
            const view = new Float32Array(wasmMemory.buffer, draw_base_ptr, 16);
            const program_index = dv.getInt32(draw_base_ptr + 64, true);
            const topology = dv.getInt32(draw_base_ptr + 64 + 4, true);
            const vertex_offset = dv.getUint32(draw_base_ptr + 64 + 8, true);
            const vertex_count = dv.getUint32(draw_base_ptr + 64 + 12, true);
            //console.log({topology, vertex_offset, vertex_count});

            const program = this.programs[program_index];

            if (current_program != program_index)
            {
                current_program = program_index;
                gl.useProgram(program.program);
                gl.uniformMatrix4fv(program.u_projection, false, proj);

                if (program_index === 1)
                {
                    const u_font = gl.getUniformLocation(program.program, "uFont");
                    gl.uniform1i(u_font, 0);
                    gl.enable(gl.BLEND);
                    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
                }
                else
                {
                    gl.disable(gl.BLEND);
                }
            }

            gl.uniformMatrix4fv(program.u_view, true, view);
            
            gl.drawArrays(topology, vertex_offset, vertex_count);
        }
    },
});