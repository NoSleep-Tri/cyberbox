/**
 * Mixed PCM Audio Processor (AudioWorklet)
 *
 * Two-input worklet that mixes microphone (inputs[0]) with system audio fed
 * via port.postMessage, converts to 16-bit PCM, and posts ~20ms chunks to
 * the main thread for Gemini Live API streaming.
 *
 * Because the system audio comes from a Tauri event (not a Web Audio source node),
 * it is pushed into this worklet via postMessage and buffered internally.
 *
 * Audio format: 16 kHz mono, raw 16-bit PCM (same as pcm-processor.js)
 */

const TARGET_CHUNK_SAMPLES = 320  // ~20ms at 16kHz
const RING_CAPACITY = TARGET_CHUNK_SAMPLES * 8

class MixedPCMProcessor extends AudioWorkletProcessor {
  constructor() {
    super()

    // Mic ring buffer (filled from process() inputs[0])
    this._micRing = new Float32Array(RING_CAPACITY)
    this._micWrite = 0
    this._micRead = 0
    this._micLen = 0

    // System-audio ring buffer (filled from postMessage)
    this._sysRing = new Float32Array(RING_CAPACITY)
    this._sysWrite = 0
    this._sysRead = 0
    this._sysLen = 0

    // Mixing weights
    this._micWeight = 1.0   // always use mic at full weight
    this._sysWeight = 1.0   // system audio at full weight; mixed by averaging if both present

    // Which channels are active
    this._hasMic = true
    this._hasSys = false   // enabled once the first system-audio-chunk arrives

    // Pre-allocated output chunk (avoid GC on audio thread)
    this._chunk = new Float32Array(TARGET_CHUNK_SAMPLES)

    this.port.onmessage = (event) => {
      const { type, data } = event.data

      if (type === 'configure') {
        // Weights are clamped to [0, 2]: 0 = muted, 1 = unity, 2 = boosted.
        // Non-numbers are silently ignored (default values from constructor are kept).
        if (typeof event.data.micWeight === 'number') {
          this._micWeight = Math.max(0, Math.min(2, event.data.micWeight))
        }
        if (typeof event.data.sysWeight === 'number') {
          this._sysWeight = Math.max(0, Math.min(2, event.data.sysWeight))
        }
        if (typeof event.data.hasMic === 'boolean') this._hasMic = event.data.hasMic
        if (typeof event.data.hasSys === 'boolean') this._hasSys = event.data.hasSys
        return
      }

      // System audio chunk received from Tauri event relay.
      // data is Float32Array (16 kHz mono) already prepared in the main thread.
      if (type === 'systemAudio' && data instanceof Float32Array) {
        this._hasSys = true
        this._writeSys(data)
      }
    }
  }

  // ── Ring buffer helpers ────────────────────────────────────────────────

  _writeMic(samples) {
    for (let i = 0; i < samples.length; i++) {
      this._micRing[this._micWrite] = samples[i]
      this._micWrite = (this._micWrite + 1) % RING_CAPACITY
      if (this._micLen < RING_CAPACITY) {
        this._micLen++
      } else {
        this._micRead = (this._micRead + 1) % RING_CAPACITY
      }
    }
  }

  _writeSys(samples) {
    for (let i = 0; i < samples.length; i++) {
      this._sysRing[this._sysWrite] = samples[i]
      this._sysWrite = (this._sysWrite + 1) % RING_CAPACITY
      if (this._sysLen < RING_CAPACITY) {
        this._sysLen++
      } else {
        this._sysRead = (this._sysRead + 1) % RING_CAPACITY
      }
    }
  }

  /** Read exactly TARGET_CHUNK_SAMPLES of mixed audio into this._chunk.
   *  If one source is absent or underrun, it contributes silence. */
  _readMixedChunk() {
    // Need enough mic samples (if mic active) OR enough sys samples (if sys active).
    const needMic = this._hasMic && this._micLen >= TARGET_CHUNK_SAMPLES
    const needSys = this._hasSys && this._sysLen >= TARGET_CHUNK_SAMPLES

    if (this._hasMic && !needMic && !this._hasSys) return false  // mic-only underrun
    if (!this._hasMic && !needSys) return false                  // sys-only underrun

    for (let i = 0; i < TARGET_CHUNK_SAMPLES; i++) {
      let micSample = 0
      if (this._micLen > 0) {
        micSample = this._micRing[this._micRead]
        this._micRead = (this._micRead + 1) % RING_CAPACITY
        this._micLen--
      }

      let sysSample = 0
      if (this._sysLen > 0) {
        sysSample = this._sysRing[this._sysRead]
        this._sysRead = (this._sysRead + 1) % RING_CAPACITY
        this._sysLen--
      }

      // Mix: apply configured weights when both sources active;
      // fall back to single-source pass-through when only one is active.
      let mixed
      const activeSources = (this._hasMic ? 1 : 0) + (this._hasSys ? 1 : 0)
      if (activeSources === 2) {
        const weightSum = this._micWeight + this._sysWeight
        if (weightSum > 0) {
          mixed = (micSample * this._micWeight + sysSample * this._sysWeight) / weightSum
        } else {
          mixed = (micSample + sysSample) * 0.5  // fallback: both weights zero
        }
      } else if (this._hasMic) {
        mixed = micSample
      } else {
        mixed = sysSample
      }

      this._chunk[i] = Math.max(-1, Math.min(1, mixed))
    }
    return true
  }

  // ── AudioWorklet process() ─────────────────────────────────────────────

  process(inputs) {
    // Write mic samples into mic ring buffer.
    const micInput = inputs[0]
    if (this._hasMic && micInput && micInput.length > 0 && micInput[0] && micInput[0].length > 0) {
      this._writeMic(micInput[0])
    }

    // Flush complete chunks.
    while (true) {
      const minReady = this._hasMic
        ? this._micLen >= TARGET_CHUNK_SAMPLES
        : this._sysLen >= TARGET_CHUNK_SAMPLES

      if (!minReady) break
      if (!this._readMixedChunk()) break

      const pcmData = new Int16Array(TARGET_CHUNK_SAMPLES)
      for (let i = 0; i < TARGET_CHUNK_SAMPLES; i++) {
        const s = this._chunk[i]
        pcmData[i] = s < 0 ? s * 0x8000 : s * 0x7fff
      }
      this.port.postMessage({ type: 'audioData', data: pcmData }, [pcmData.buffer])
    }

    return true
  }
}

registerProcessor('mixed-pcm-processor', MixedPCMProcessor)
