const Notcurses = require('./lib/notcurses')
const Plane = require('./lib/plane')
const InputEvent = require('./lib/input-event')
const Channels = require('./lib/channels')
const Visual = require('./lib/visual')
const constants = require('./lib/constants')
const binding = require('./binding')

function ncstrwidth (str, ignoreInvalidUnicode = false) {
  return binding.ncstrwidth(str, ignoreInvalidUnicode)
}

module.exports = {
  Notcurses,
  Plane,
  InputEvent,
  Channels,
  Visual,
  ncstrwidth,
  ...constants
}
