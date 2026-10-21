/**
 * PCM Audio Processor (AudioWorklet)
 *
 * Converts Float32Array input from Web Audio API to 16-bit PCM (Int16Array)
 * and sends it to the main thread for transmission to the Gemini Live API.
 *
 * Per Live API best practices:
 *   - Audio format: raw 16-bit PCM, little-endian, 16kHz, mono
 *   - Chunk size: 20–40ms recommended (320–640 samples at 16kHz)
 *
 * AudioWorklet `process()` is called with 128-sample frames. We accumulate
 * frames into ~20ms buffers (320 samples) before posting to the main thread.
 *
 * Implementation uses a pre-allocated ring buffer (Float32Array) to avoid
 * GC pressure and heap allocations on the audio thread. Overflow drops the
 * oldest samples; underrun simply skips the read.
 */

const TARGET_CHUNK_SAMPLES = 320 // ~20ms at 16kHz

// Ring buffer capacity: hold at least 4 chunks worth of samples so normal
// scheduling jitter never causes overflow.
const RING_CAPACITY = TARGET_CHUNK_SAMPLES * 4

class PCMProcessor extends AudioWorkletProcessor {
  constructor() {
    super()

    // Pre-allocated ring buffer – no dynamic allocation on the audio thread.
    this._ring = new Float32Array(RING_CAPACITY)
    this._writeIndex = 0    // next slot to write into
    this._readIndex = 0     // next slot to read from
    this._currentLength = 0 // number of valid samples currently in the buffer

    // Pre-allocated output chunk to avoid per-flush allocation in process().
    this._chunk = new Float32Array(TARGET_CHUNK_SAMPLES)

    this.port.onmessage = (event) => {
      if (event.data.type === 'configure') {
        console.log('PCM Processor configured')
        // TODO: Apply incoming configuration when runtime-configurable state is needed.
        // Expected keys in event.data.config:
        //   - chunkSamples  {number} – override TARGET_CHUNK_SAMPLES; update this._chunk
        //                              (new Float32Array(chunkSamples)) and resize the ring
        //                              buffer accordingly (new Float32Array(chunkSamples * 4))
        //   - sampleRate    {number} – informational; the AudioWorklet context sampleRate is
        //                              already available via the global `sampleRate` variable
        //                              and does not need to be stored separately
        // Example wiring:
        //   const { chunkSamples } = event.data.config ?? {}
        //   if (chunkSamples && chunkSamples !== this._chunk.length) {
        //     this._chunk = new Float32Array(chunkSamples)
        //     this._ring  = new Float32Array(chunkSamples * 4)
        //     this._writeIndex = this._readIndex = this._currentLength = 0
        //   }
      }
    }
  }

  /**
   * Write samples into the ring buffer.
   * Overflow handling: drop the oldest samples to make room (the write
   * pointer advances past the read pointer, then the read pointer is
   * moved forward to stay consistent).
   */
  _write(samples) {
    for (let i = 0; i < samples.length; i++) {
      this._ring[this._writeIndex] = samples[i]
      this._writeIndex = (this._writeIndex + 1) % RING_CAPACITY

      if (this._currentLength < RING_CAPACITY) {
        this._currentLength++
      } else {
        // Overflow: oldest sample is lost; advance read pointer.
        this._readIndex = (this._readIndex + 1) % RING_CAPACITY
      }
    }
  }

  /**
   * Read exactly TARGET_CHUNK_SAMPLES from the ring buffer into this._chunk.
   * Returns true if there were enough samples, false on underrun (skip).
   */
  _readChunk() {
    if (this._currentLength < TARGET_CHUNK_SAMPLES) {
      return false // underrun – skip this flush cycle
    }
    for (let i = 0; i < TARGET_CHUNK_SAMPLES; i++) {
      this._chunk[i] = this._ring[this._readIndex]
      this._readIndex = (this._readIndex + 1) % RING_CAPACITY
    }
    this._currentLength -= TARGET_CHUNK_SAMPLES
    return true
  }

  process(inputs, outputs, parameters) {
    const input = inputs[0]

    if (input && input.length > 0) {
      const inputChannel = input[0] // mono

      if (inputChannel && inputChannel.length > 0) {
        // Write incoming frame into the ring buffer.
        this._write(inputChannel)

        // Flush complete chunks while enough samples are buffered.
        while (this._currentLength >= TARGET_CHUNK_SAMPLES) {
          if (!this._readChunk()) break // underrun guard (should not happen here)

          const pcmData = new Int16Array(TARGET_CHUNK_SAMPLES)
          for (let i = 0; i < TARGET_CHUNK_SAMPLES; i++) {
            // Clamp float32 to [-1, 1] then convert to int16
            const sample = Math.max(-1, Math.min(1, this._chunk[i]))
            pcmData[i] = sample < 0 ? sample * 0x8000 : sample * 0x7fff
          }

          this.port.postMessage({ type: 'audioData', data: pcmData }, [pcmData.buffer])
        }
      }
    }

    // Return true to keep the processor alive
    return true
  }
}

registerProcessor('pcm-processor', PCMProcessor)