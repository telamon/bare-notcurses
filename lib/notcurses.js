const binding = require('../binding')
const InputEvent = require('./input-event')
const Plane = require('./plane')
const { uncaught } = require('./util')
const { NCMICE_NO_EVENTS } = require('./constants')

class Notcurses {
  #handle
  #stdplane

  constructor (opts = {}) {
    this.#handle = binding.init(opts.flags || 0)

    if (opts.oninput) {
      this.inputStart(opts.oninput)
    }

    if (opts.uncaught) {
      uncaught(err => {
        this.destroy()
        console.error('Terminal restored due to uncaught exception:\n', err)
      })
    }
  }

  get _handle () {
    return this.#handle
  }

  /** @type {Plane} */
  get stdplane () {
    if (!this.#stdplane) {
      this.#stdplane = new Plane(this, binding.stdplane(this.#handle))
    }

    return this.#stdplane
  }

  get pixelSupport () {
    return binding.pixelSupport(this.#handle) !== binding.NCPIXEL_NONE
  }

  inputStart (handler, miceEvents = NCMICE_NO_EVENTS) {
    if (typeof handler !== 'function') throw new Error('Callback expected')

    function oninput (eventHandle) {
      handler(new InputEvent(eventHandle))
    }

    binding.inputStart(this.#handle, oninput, miceEvents)
  }

  inputStop () {
    binding.inputStop(this.#handle)
  }

  render () {
    return binding.render(this.#handle)
  }

  destroy () {
    if (this.#handle == null) throw new Error('already destroyed')

    binding.destroy(this.#handle)
    this.#handle = null
  }

  get destroyed () { return !!this.#handle }

  [Symbol.dispose] () { return this.destroy() }
}

module.exports = Notcurses
