const binding = require('../binding')
const { NCSTYLE_NONE } = require('./constants')
const Channels = require('./channels')
const { inspect } = require('./util')

/** @typedef {import('./notcurses')} Notcurses */

class Plane {
  #handle
  #channels

  /** @param {Plane} parent */
  constructor (parent, opts = {}) {
    // plane allocated elswhere, wrap handle
    if (opts instanceof ArrayBuffer) {
      this.#handle = opts
      return
    }

    // Allocate from opts

    opts.rows ||= opts.height
    opts.cols ||= opts.width

    const {
      name,
      flags,
      x, y,
      rows, cols,
      marginBottom, marginRight
    } = opts

    let onresize

    if (typeof opts.onresize === 'function') {
      onresize = () => opts.onresize(this)
    }

    this.#handle = binding.planeCreate(
      parent._handle,
      y || 0,
      x || 0,
      rows || 0,
      cols || 0,
      flags || 0,
      marginBottom || 0,
      marginRight || 0,
      name,
      onresize
    )
  }

  get _handle () { // oops
    return this.#handle
  }

  get y () {
    return binding.getPlaneY(this.#handle)
  }

  set y (value) {
    this.move(value, this.x)
  }

  get x () {
    return binding.getPlaneX(this.#handle)
  }

  set x (value) {
    this.move(this.y, value)
  }

  get dimY () {
    return binding.getPlaneDimY(this.#handle)
  }

  get dimX () {
    return binding.getPlaneDimX(this.#handle)
  }

  get name () {
    return binding.getPlaneName(this.#handle)
  }

  set name (value) {
    binding.setPlaneName(this.#handle, value)
  }

  get cursorY () {
    return binding.getPlaneCursorY(this.#handle)
  }

  set cursorY (value) {
    const err = this.cursorMove(value, -1)
    if (err < 0) throw new Error('cursorMove out of bounds')
  }

  get cursorX () {
    return binding.getPlaneCursorX(this.#handle)
  }

  set cursorX (value) {
    const err = this.cursorMove(-1, value)
    if (err < 0) throw new Error('cursorMove out of bounds')
  }

  get styles () {
    return binding.getPlaneStyles(this.#handle)
  }

  set styles (ncstyle) {
    return binding.setPlaneStyles(this.#handle, ncstyle)
  }

  /** @returns {Channels} */
  get channels () {
    if (!this.#channels) {
      this.#channels = new Channels({
        get: () => binding.getPlaneChannels(this.#handle),
        set: v => { this.channels = v }
      })
    }

    return this.#channels
  }

  set channels (value) {
    // TODO: bug; plane.channels = a; plane.chanenls === a; => false
    const bn = Channels.from(value).value
    binding.setPlaneChannels(this.#handle, bn)
  }

  move (y, x) {
    return binding.planeMoveYX(this.#handle, y, x)
  }

  resize (rows = -1, cols = -1) {
    return binding.planeResizeSimple(this.#handle, rows, cols)
  }

  erase () {
    binding.planeErase(this.#handle)
  }

  setBase (egc = ' ', styles = NCSTYLE_NONE, channels = 0n) {
    binding.planeSetBase(this.#handle, egc, styles, Channels.from(channels).value)
  }

  putstr (str, y = -1, x = -1) {
    return binding.planePutstrYX(this.#handle, y, x, str)
  }

  vline (egc, len, styles = NCSTYLE_NONE, channels = 0n) {
    return binding.planeVLine(this.#handle, egc, len, styles, Channels.from(channels).value)
  }

  cursorMove (y = -1, x = -1) {
    return binding.planeCursorMoveYX(this.#handle, y, x)
  }

  home () {
    this.cursorMove(0, 0)
  }

  moveTop () {
    binding.planeMoveTop(this.#handle)
  }

  moveBottom () {
    binding.planeMoveBottom(this.#handle)
  }

  /** @param {Plane} dstPlane */
  reparent (dstPlane) {
    return binding.planeReparentFamily(this.#handle, dstPlane.#handle)
  }

  /** @param {Plane} dstPlane */
  mergeDown (dstPlane) {
    return binding.planeMergedown(this.#handle, dstPlane.#handle)
  }

  perimeterRounded (styleMask = NCSTYLE_NONE, channels = 0n, ctlword = 0) {
    return binding.planePerimeter(this.#handle, 0, styleMask, Channels.from(channels).value, ctlword)
  }

  perimeterDouble (styleMask = NCSTYLE_NONE, channels = 0n, ctlword = 0) {
    return binding.planePerimeter(this.#handle, 1, styleMask, Channels.from(channels).value, ctlword)
  }

  contents (x = -1, y = -1, lenX = 0, lenY = 0) {
    return binding.planeContents(this.#handle, x, y, lenX, lenY)
  }

  get pixelGeom () {
    return binding.planePixelGeom(this._handle)
  }

  destroy (family = false) {
    if (family) binding.planeFamilyDestroy(this.#handle)
    else binding.planeDestroy(this.#handle)

    this.#handle = null
  }

  [Symbol.dispose] () { this.destroy() }

  [inspect] () {
    return {
      __proto__: { constructor: Plane },
      _handle: this.#handle,
      flags: 'todo',
      name: this.name,
      styles: this.styles,
      y: this.y,
      x: this.x,
      dimY: this.dimY,
      dimX: this.dimX,
      cursorY: this.cursorY,
      cursorX: this.cursorX,
      channels: this.channels
    }
  }
}

module.exports = Plane
