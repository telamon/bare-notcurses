const N = require('.')

const nc = new N.Notcurses({ uncaught: true })

const { width, height } = nc.stdplane

const blueBg = new N.Channels()
blueBg.bgIdx = 8

const { stdplane } = nc

stdplane.setBase(' ', N.NCSTYLE_NONE, blueBg)

// stdplane.channels.bgIdx = 4

nc.inputStart(oninput, N.NCMICE_ALL_EVENTS)

const hello = nc.createPlane({
  rows: 10,
  cols: 40
})

hello.channels.bgIdx = 6
hello.channels.fgIdx = 10

hello.erase()

hello.move(2, 1)

hello.cursorMove(1, 1)
hello.styles = N.NCSTYLE_BOLD | N.NCSTYLE_UNDERCURL
hello.putstr('Hello World')
hello.styles = N.NCSTYLE_NONE

hello.channels = 0

hello.cursorMove(3, 1)
hello.putstr('Press ')

const pink = new N.Channels()
pink.fgRgb = 0xff0050

hello.channels = pink
hello.putstr('"q"')
hello.channels = 0

hello.putstr(' to exit')
// ncplane_set_styles(plane, NCSTYLE_NONE);

// setup plane that displays input event details (bottom line)
const info = nc.createPlane({
  name: 'input inspector',
  rows: 1,
  cols: width
})
info.move(height - 1, 0)

nc.render()

/** @param {N.InputEvent} ev */
function oninput (ev) {
  if (ev.text === 'q') {
    nc.destroy()
    return
  }

  const mods = []
  if (ev.alt) mods.push('ALT')
  if (ev.shift) mods.push('SHIFT')
  if (ev.ctrl) mods.push('CTRL')
  if (ev.meta) mods.push('META')
  if (ev.hyper) mods.push('HYPER')
  if (ev.capslock) mods.push('CAPSLCK')
  if (ev.numlock) mods.push('NUMLCK')
  if (ev.mouse) mods.push('mouse')

  info.erase()
  info.putstr(`Event: ${ev.id} ${mods.join('|')} '${ev.utf8}' ${ev.type} ${ev.x} ${ev.y} ${ev.xpx} ${ev.ypx}`)
  nc.render()
}
