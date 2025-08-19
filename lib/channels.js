const binding = require('../binding')

class Channels {
  #stack = []
  #proxy = null
  #value = 0n
  constructor (channels = 0n) {
    if (
      typeof channels === 'object' &&
      typeof channels.get === 'function' &&
      typeof channels.set === 'function'
    ) {
      // proxy all reads/writes directly to plane
      this.#proxy = channels
    } else if (typeof channels === 'bigint') {
      this.#value = channels // default
    } else {
      throw new Error('unsupported value' + channels)
    }
  }

  get value () {
    return this.#proxy ? this.#proxy.get() : this.#value
  }

  set value (v) {
    if (this.#proxy) {
      this.#proxy.set(v)
    } else {
      this.#value = v
    }
  }

  // foreground

  get fg () {
    return binding.getChannelFg(this.value)
  }

  set fg (value) {
    this.value = binding.combineChannels(value, this.bg)
  }

  get fgRgb () {
    return binding.getChannelRGB(this.fg)
  }

  set fgRgb (rgb) {
    this.fg = binding.setChannelRGB(this.fg, rgb)
  }

  get fgIdx () {
    return binding.getChannelPalindex(this.fg)
  }

  set fgIdx (idx) {
    this.fg = binding.setChannelPalindex(this.fg, idx)
  }

  get isFgRgb () {
    return binding.isChannelRGB(this.fg)
  }

  get isFgIndexed () {
    return binding.isChannelIndexed(this.fg)
  }

  get isFgDefault () {
    return binding.isChannelDefault(this.fg)
  }

  // background

  get bg () {
    return binding.getChannelBg(this.value)
  }

  set bg (value) {
    this.value = binding.combineChannels(this.fg, value)
  }

  get bgRgb () {
    return binding.getChannelRGB(this.bg)
  }

  set bgRgb (rgb) {
    this.bg = binding.setChannelRGB(this.bg, rgb)
  }

  get bgIdx () {
    return binding.getChannelPalindex(this.bg)
  }

  set bgIdx (idx) {
    this.bg = binding.setChannelPalindex(this.bg, idx)
  }

  get isBgRgb () {
    return binding.isChannelRGB(this.bg)
  }

  get isBgIndexed () {
    return binding.isChannelIndexed(this.bg)
  }

  get isBgDefault () {
    return binding.isChannelDefault(this.bg)
  }

  get reverse () {
    return new Channels(binding.reverseChannels(this.value))
  }

  push () {
    this.#stack.push(this.value)
  }

  pop () {
    this.value = this.#stack.pop()
  }

  static from (fgChannel, bgChannel) {
    // combine channels
    if (typeof bgChannel === 'number' && typeof fgChannel === 'number') {
      return new Channels(binding.combineChannels(fgChannel, bgChannel))
    }

    if (typeof fgChannel === 'bigint') return new Channels(fgChannel)

    // lossy bigint
    if (typeof fgChannel === 'number') return new Channels(BigInt(fgChannel))

    // dupe
    if (fgChannel instanceof Channels) return new Channels(fgChannel.value)

    // null-channel
    if (!fgChannel) return new Channels(0n)

    throw new Error('failed to deduce channels')
  }
}

module.exports = Channels
