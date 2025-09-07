const binding = require('../binding')
const { NCSTYLE_NONE } = require('./constants')
const Channels = require('./channels')
const { inspect } = require('./util')

/** @typedef {import('./notcurses')} Notcurses */

class Plane {
  #handle

  /** @param {Notcurses} notcurses */
  constructor (notcurses, opts = {}) {
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
      notcurses._handle,
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

  get x () {
    return binding.getPlaneX(this.#handle)
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

  get cursorX () {
    return binding.getPlaneCursorX(this.#handle)
  }

  get styles () {
    return binding.getPlaneStyles(this.#handle)
  }

  set styles (ncstyle) {
    return binding.setPlaneStyles(this.#handle, ncstyle)
  }

  get channels () {
    return new Channels({
      get: () => binding.getPlaneChannels(this.#handle),
      set: v => { this.channels = v }
    })
  }

  set channels (value) {
    binding.setPlaneChannels(this.#handle, Channels.from(value).value)
  }

  move (y, x) {
    binding.planeMoveYX(this.#handle, y, x)
  }

  resize (rows, cols) {
    binding.planeResizeSimple(this.#handle, rows, cols)
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

  cursorMove (y, x) {
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
  mergeDown (dstPlane) {
    return binding.planeMergedown(this.#handle, dstPlane.#handle)
  }

  perimeterRounded (styleMask = NCSTYLE_NONE, channels = 0n, ctlword = 0) {
    return binding.planePerimiter(this.#handle, 0, styleMask, Channels.from(channels).value, ctlword)
  }

  perimeterDouble (styleMask = NCSTYLE_NONE, channels = 0n, ctlword = 0) {
    return binding.planePerimiter(this.#handle, 1, styleMask, Channels.from(channels).value, ctlword)
  }

  destroy () {
    binding.planeDestroy(this.#handle)
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
