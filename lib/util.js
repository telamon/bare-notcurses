const isBare = !!globalThis.Bare

const runtime = isBare
  ? globalThis.Bare
  : globalThis.process

function uncaught (fn) {
  runtime.once('uncaughtException', fn)
}

const inspect = isBare
  ? Symbol.for('bare.inspect')
  : Symbol.for('nodejs.util.inspect.custom')

module.exports = {
  uncaught,
  inspect
}
