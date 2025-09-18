class A extends AudioWorkletProcessor {
  constructor({processorOptions}) {
    super();
    const {sab, count} = processorOptions;
    this.buf = [];
    for (let i = 0; i < count; ++i) {
      this.buf.push(new Float32Array(sab, i * 4 * 128, 128));
    }
    this.count = count;
    this.i = 0;
  }

  process(_, outputs) {
    const channel = outputs[0][0];
    channel.set(this.buf[this.i]);
    this.port.postMessage(this.buf[this.i]);
    this.i = (this.i + 1) % this.count;
    return true;
  }
}

registerProcessor("rack", A);
