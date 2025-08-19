const binding = require('../binding')
const Plane = require('./plane')
const {
  NCSCALE_STRETCH,
  NCBLIT_DEFAULT
} = require('./constants')

/** @typedef {import('./notcurses')} Notcurses */

class Visual {
  #nc
  #handle

  /** @param {Notcurses} notcurses */
  constructor (notcurses, data, width, height, bytesPerPixel = 4) {
    if (!Buffer.isBuffer(data)) throw new Error('expected buffer')

    this.#nc = notcurses

    this.#handle = binding.visualCreate(
      data.buffer,
      data.byteOffset,
      data.byteLength,
      width,
      height,
      bytesPerPixel
    )
  }

  /**
   * @param {Plane} dstPlane
   */
  blit (dstPlane, scaling = NCSCALE_STRETCH, blitter = NCBLIT_DEFAULT, flags = 0) {
    // TODO: the disabled lines in this function refer to
    // the feature of passing an undefined dstPlane (`ncvisual_options.n = NULL`)
    // I can't figure out how it's intended to work.

    // this.plane = this.plane || dstPlane || undefined

    binding.visualBlit(
      this.#nc._handle,
      this.#handle,
      dstPlane._handle, // this.plane && this.plane._handle,
      0, // y
      0, // x
      scaling,
      blitter,
      flags
    )

    /*
    // a plane was created during blit
    if (planeHandle) {
      this.plane = new Plane(this.#nc, planeHandle)
    }
    */
  }

  destroy () {
    binding.visualDestroy(this.#handle)
    this.#handle = null

    if (this.plane) {
      this.plane.destroy()
      this.plane = null
    }
  }

  [Symbol.dispose] () { this.destroy() }
}

module.exports = Visual
