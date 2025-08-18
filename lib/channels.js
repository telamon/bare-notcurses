const binding = require('../binding')

class Channels {
  constructor (channels = 0n) {
    this.value = channels
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

  static from (fgChannel, bgChannel) {
    return new Channels(binding.combineChannels(fgChannel, bgChannel))
  }
}

module.exports = Channels
