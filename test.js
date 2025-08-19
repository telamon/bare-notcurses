const test = require('brittle')
const { Notcurses, Plane, Channels } = require('.')

// NOTE: without redirecting rendering
// and synthesizing input events
// - there's a limited amount things we can test.
//
// see examples.

test('init and restore term', () => {
  const nc = new Notcurses()
  nc.destroy()
})

// can't test; use subproc maybe
test.skip('input event', async t => {
  let oninput
  const p = new Promise(resolve => { oninput = resolve })

  const nc = new Notcurses({ oninput })

  const event = await p
  console.error('event received', event)

  nc.destroy()
})

test('stdplane', t => {
  const nc = new Notcurses()
  const std = nc.stdplane

  t.is(std.name, 'std')
  std.name = 'bob'

  const { y, x, dimY, dimX, name } = std

  nc.destroy()

  t.is(y, 0)
  t.is(x, 0)
  t.not(dimY, 0)
  t.not(dimX, 0)
  t.is(name, 'bob')
})

test('create plane', t => {
  const nc = new Notcurses()

  const plane = new Plane(nc, {
    name: 'arnold',
    y: 2,
    x: 10,
    height: 5,
    width: 20
  })

  const { y, x, dimY, dimX, name } = plane

  plane.move(7, 7)
  plane.resize(10, 10)

  const { y: y2, x: x2, dimY: h2, dimX: w2 } = plane

  const { stdplane } = nc
  plane.mergeDown(stdplane)

  nc.destroy()

  t.is(y, 2)
  t.is(x, 10)
  t.is(dimY, 5)
  t.is(dimX, 20)
  t.is(name, 'arnold')

  t.is(y2, 7)
  t.is(x2, 7)
  t.is(h2, 10)
  t.is(w2, 10)
})

test('ncchannels', t => {
  const c = new Channels()
  t.is(c.value, 0n)
  t.is(c.fgRgb, 0, 'initial fgrgb')
  t.is(c.isFgDefault, true, 'is default')

  c.fgRgb = 0xff00ff

  t.not(c.value, 0n, 'value changed')
  t.is(c.fgRgb, 0xff00ff, 'fg rgb set')
  t.is(c.isFgRgb, true, 'fg is rgb')
  t.is(c.isFgIndexed, false, 'fg not indexed')
  t.is(c.isFgDefault, false, 'fg not default')
  t.is(c.isBgDefault, true, 'bg default')
})
