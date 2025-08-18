const binding = require('../binding')
const { inspect } = require('./util')
class InputEvent {
  #handle

  constructor (handle) {
    this.#handle = handle
  }

  get id () {
    return binding.getEventId(this.#handle)
  }

  get type () {
    // NOTE: "gnome-terminal" always reports NCTYPE_UNKNOWN=0
    const t = binding.getEventType(this.#handle)

    switch (t) {
      case binding.NCTYPE_UNKNOWN:
        return 'unknown'
      case binding.NCTYPE_PRESS:
        return 'pressed'
      case binding.NCTYPE_REPEAT:
        return 'repeated'
      case binding.NCTYPE_RELEASE:
        return 'released'
      default:
        throw new Error(`Unexpected NCTYPE<${t}>`)
    }
  }

  get y () {
    return binding.getEventY(this.#handle)
  }

  get x () {
    return binding.getEventX(this.#handle)
  }

  get ypx () {
    return binding.getEventYpx(this.#handle)
  }

  get xpx () {
    return binding.getEventXpx(this.#handle)
  }

  get utf8 () {
    return binding.getEventUtf8(this.#handle)
  }

  get modifiers () {
    return binding.getEventModifiers(this.#handle)
  }

  get alt () {
    return this.modifiers & binding.NCKEY_MOD_ALT
  }

  get ctrl () {
    return this.modifiers & binding.NCKEY_MOD_CTRL
  }

  get shift () {
    return this.modifiers & binding.NCKEY_MOD_SHIFT
  }

  get meta () {
    return this.modifiers & binding.NCKEY_MOD_META
  }

  get super () {
    return this.modifiers & binding.NCKEY_MOD_SUPER
  }

  get hyper () {
    return this.modifiers & binding.NCKEY_MOD_HYPER
  }

  get capslock () {
    return this.modifiers & binding.NCKEY_MOD_CAPSLOCK
  }

  get numlock () {
    return this.modifiers & binding.NCKEY_MOD_NUMLOCK
  }

  get text () {
    const arraybuffer = binding.getEventEffText(this.#handle)
    const view = new Uint32Array(arraybuffer)

    const end = view.findIndex(i => i === 0)

    const buf = Buffer.from(arraybuffer)
    const text = buf.subarray(0, end).toString('utf8')
    return text
  }

  [inspect] () {
    return {
      __proto__: { constructor: InputEvent },
      _handle: this.#handle,
      id: this.id,
      type: this.type,
      y: this.y,
      x: this.x,
      ypx: this.ypx,
      xpx: this.xpx,
      utf8: this.utf8,
      text: this.text,
      modifiers: this.modifiers,
      alt: this.alt,
      ctrl: this.ctrl,
      shift: this.shift,
      meta: this.meta,
      hyper: this.hyper,
      capslock: this.capslock,
      numlock: this.numlock
    }
  }
}

module.exports = InputEvent
