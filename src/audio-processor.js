class AudioProcessor extends AudioWorkletProcessor {
  constructor({processorOptions}) {
    super();
    const {pcm_sab, bufcount} = processorOptions;
    this.pcm_buf = [];
    for (let i = 0; i < bufcount; ++i) {
      this.pcm_buf.push(new Float32Array(pcm_sab, i * 4 * 128, 128));
    }
    this.bufcount = bufcount;
    this.index = 0;
  }

  process(inputs, outputs) {
    const output = outputs[0];
    const channel = output[0];
    channel.set(this.pcm_buf[this.index]);
    this.port.postMessage(this.pcm_buf[this.index]);
    this.index = (this.index + 1) % this.bufcount;
    return true;
  }
}

registerProcessor("rack", AudioProcessor);
