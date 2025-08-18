const binding = require('../binding')
const Channels = require('./channels')
const { inspect } = require('./util')

class Plane {
  #handle

  constructor (ncHandle, handle = undefined, opts = {}) {
    if (handle) {
      this.#handle = handle
    } else {
      opts.rows ||= opts.height
      opts.cols ||= opts.width

      const {
        x, y,
        rows, cols,
        flags,
        marginBottom, marginRight,
        name
      } = opts

      this.#handle = binding.createPlane(
        ncHandle,
        y || 0,
        x || 0,
        rows || 0,
        cols || 0,
        flags || 0,
        marginBottom || 0,
        marginRight || 0,
        name
      )
    }
  }

  get y () {
    return binding.getPlaneY(this.#handle)
  }

  get x () {
    return binding.getPlaneX(this.#handle)
  }

  get height () {
    return binding.getPlaneHeight(this.#handle)
  }

  get width () {
    return binding.getPlaneWidth(this.#handle)
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
    return new Channels(binding.getPlaneChannels(this.#handle))
  }

  set channels (value) {
    if (!value) value = BigInt(0)
    if (typeof value === 'number') value = BigInt(value)
    if (value instanceof Channels) value = value.value

    binding.setPlaneChannels(this.#handle, value)
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

  setBase (egc, styleMask, channels) {
    binding.planeSetBase(this.#handle, egc, styleMask, channels)
  }

  putstr (str, y = -1, x = -1) {
    return binding.planePutstrYX(this.#handle, y, x, str)
  }

  vline (egc, len, styleMask = 0, channels = 0) {
    return binding.planeVLine(this.#handle, egc, len, styleMask, channels)
  }

  cursorMove (y, x) {
    return binding.planeCursorMoveYX(this.#handle, y, x)
  }

  home () {
    this.cursorMove(0, 0)
  }

  moveTop () {
    return binding.planeMoveTop(this.#handle)
  }

  moveBottom () {
    return binding.planeMoveBottom(this.#handle)
  }

  [inspect] () {
    return {
      __proto__: { constructor: Plane },
      _handle: this.#handle,
      flags: 'todo',
      name: this.name,
      styles: this.styles,
      y: this.y,
      x: this.x,
      height: this.height,
      width: this.width,
      cursorY: this.cursorY,
      cursorX: this.cursorX
    }
  }
}

module.exports = Plane
