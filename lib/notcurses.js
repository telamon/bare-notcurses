const binding = require('../binding')
const InputEvent = require('./input-event')
const Plane = require('./plane')
const { uncaught } = require('./util')
const { NCMICE_NO_EVENTS } = require('./constants')

class Notcurses {
  #handle
  #stdplane

  constructor (opts = {}) {
    this.#handle = binding.init()

    if (opts.oninput) {
      this.inputStart(opts.oninput)
    }

    if (opts.uncaught) {
      uncaught(err => {
        this.destroy()
        console.error('Terminal restored due to uncaught exception\n', err)
      })
    }
  }

  /** @type {Plane} */
  get stdplane () {
    if (!this.#stdplane) {
      this.#stdplane = new Plane(this.#handle, binding.stdplane(this.#handle))
    }

    return this.#stdplane
  }

  inputStart (callback, miceEvents = NCMICE_NO_EVENTS) {
    if (typeof callback !== 'function') throw new Error('Callback expected')

    function oninput (eventHandle) {
      callback(new InputEvent(eventHandle))
    }

    binding.inputStart(this.#handle, oninput, miceEvents)
  }

  inputStop () {
    binding.inputStop(this.#handle)
  }

  createPlane (opts = {}) {
    return new Plane(this.#handle, undefined, opts)
  }

  render () {
    return binding.render(this.#handle)
  }

  destroy () {
    if (this.#handle == null) throw new Error('already destroyed')

    binding.destroy(this.#handle)
    this.#handle = null
  }

  [Symbol.dispose] () { return this.destroy() }
}

module.exports = Notcurses
