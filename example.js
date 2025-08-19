const N = require('.')
const { version } = require('./package.json')
const { Notcurses, Channels, Plane, Visual } = N

const nc = new Notcurses({ uncaught: true })

// preconfigure some styled channels
const rootStyle = new Channels()
rootStyle.bgIdx = 4
rootStyle.fgIdx = 15

const boxBg = new Channels()
boxBg.bgIdx = 4
boxBg.fgIdx = 15
// boxBg.value = boxBg.reverse.value

const pink = new Channels()
pink.fgRgb = 0xff0080

// Set global draw style
const { stdplane } = nc
const { dimX: rootW, dimY: rootH } = nc.stdplane
stdplane.setBase(' ', N.NCSTYLE_NONE, rootStyle)

testStyles()
testInput()
testImage()
drawBanner()

// Render initial screen
nc.render()

function testStyles () {
  // initialize & draw a hello-box
  const plane = new Plane(nc, {
    rows: 10,
    cols: 40,
    flags: N.NCPLANE_OPTION_VSCROLL // enable linebreaks ('\n')
  })

  plane.setBase(' ', N.NCSTYLE_NONE, rootStyle.reverse)

  // reconfigure current style
  // plane.channels.bgIdx = 6
  // plane.channels.fgIdx = 10

  plane.move(2, 4)

  plane.cursorMove(2, 2)
  plane.styles = N.NCSTYLE_BOLD
  plane.putstr('bare-notcurses')
  plane.styles = N.NCSTYLE_NONE
  plane.putstr(` - v${version}`)

  plane.cursorMove(1, 2)
  plane.styles = N.NCSTYLE_BOLD
  plane.channels.fgIdx = 0
  plane.putstr('notcurses')
  plane.channels.fgIdx = rootStyle.reverse.fgIdx // restore
  plane.styles = N.NCSTYLE_NONE
  plane.putstr(` - v${N.NOTCURSES_VERSION}`)
  plane.channels = 0 // clear style

  // multi-style line
  plane.cursorMove(4, 2)
  plane.putstr('Press ')
  plane.channels = pink
  plane.putstr('"q"')
  plane.channels = 0
  plane.putstr(' to ')
  plane.styles = N.NCSTYLE_UNDERCURL
  plane.putstr('quit')
  plane.styles = 0

  if (globalThis.Bare) { // eslint-disable-line no-constant-condition
    const { getEnv, platform, arch, release } = require('bare-os')

    plane.cursorMove(7, 2)
    plane.putstr(`term - ${getEnv('TERM')}\n  platform - ${platform()} ${release()} ${arch()}`)
  }
}

function testInput () {
  // setup plane that displays input event details (bottom line)
  const plane = new Plane(nc, {
    name: 'input inspector',
    rows: 1,
    cols: rootW
  })
  plane.setBase(' ', N.NCSTYLE_NONE, rootStyle)
  plane.move(rootH - 1, 0) // move to bottom

  // Register input handler + mouse support
  nc.inputStart(oninput, N.NCMICE_ALL_EVENTS)

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

    plane.erase()
    plane.putstr(` Event: ${ev.id} ${mods.join('|')} '${ev.utf8}' ${ev.type} ${ev.x} ${ev.y} ${ev.xpx} ${ev.ypx}`)

    nc.render() // rerender changes
  }
}

function testImage () {
  const webp = require('bare-webp')

  const image = require('./demo/polar-chunli-sqr.webp', { with: { type: 'binary' } })

  const { data, width, height } = webp.decode(image)

  // test default sexblitter

  const p1 = new Plane(nc, { rows: 20, cols: 40 })
  p1.move(0, stdplane.dimX - p1.dimX - 5)

  const vis = new Visual(nc, data, width, height)
  vis.blit(p1, N.NCSCALE_STRETCH, N.NCBLIT_3x2)
  vis.destroy()

  const info1 = new Plane(nc, { rows: 4, cols: 19 })

  info1.setBase(' ', N.NCSTYLE_NONE, boxBg)
  info1.move(p1.y + 2, p1.x - info1.dimX - 1)
  info1.cursorMove(1, 2)
  info1.putstr('Default 3x2')
  info1.cursorMove(2, 2)
  info1.putstr('blitter')
  info1.perimiterRounded()

  const info2 = new Plane(nc, { rows: 4, cols: 20 })
  info2.setBase(' ', N.NCSTYLE_NONE, boxBg)
  info2.perimiterRounded()

  if (!nc.pixelSupport) { // check pixel-support
    info2.move(p1.y + p1.dimY + 5, p1.x + 3)
    info2.cursorMove(1, 2)
    info2.putstr('pixelblitter')
    info2.cursorMove(2, 2)
    info2.putstr('not supported')
    return
  }

  // test pixel-blitter

  const image2 = require('./demo/polar-chunli-pix.webp', { with: { type: 'binary' } })

  const pixels = webp.decode(image2)

  const vis2 = new Visual(nc, pixels.data, pixels.width, pixels.height)

  const p2 = new Plane(nc, { rows: 10, cols: 20 })
  p2.move(p1.y + p1.dimY + 1, p1.x)

  vis2.blit(p2, N.NCSCALE_SCALE_HIRES, N.NCBLIT_PIXEL)

  // draw info box
  info2.move(p2.y + 4, p2.x - info2.dimX - 1)
  info2.cursorMove(1, 2)
  info2.putstr('Pixelblitter')
  info2.cursorMove(2, 2)
  info2.putstr('supported')
}


function drawBanner () {
  const ascii = `
                    _
      _ __    ___  | |_  ___  _   _  _ __  ___   ___  ___
     | '_ \\  / _ \\ | __|/ __|| | | || '__|/ __| / _ \\/ __|
     | | | || (_) || |_| (__ | |_| || |   \\__ \\|  __/\\__ \\
     |_| |_| \\___/  \\__|\\___| \\__,_||_|   |___/ \\___||___/
  `.split('\n').map(l => l.replace(/^ {4}/, '')).filter(l => l).join('\n')

  const rows = ascii.split('\n').length
  const cols = ascii.split('\n').sort((a, b) => b.length - a.length)[0].length

  const banner = new Plane(nc, {
    rows: rows + 2,
    cols: cols + 2,
    flags: N.NCPLANE_OPTION_VSCROLL
  })
  banner.move(15, 4)
  const ch = Channels.from(rootStyle)
  ch.fgIdx = 12
  banner.setBase(' ', N.NCSTYLE_NONE, ch)

  banner.putstr(ascii)

  banner.cursorMove(rows, Math.round(cols / 2))
  banner.putstr('for bare!')
  //banner.perimiterDouble()
}
