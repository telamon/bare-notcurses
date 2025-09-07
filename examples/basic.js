const {
  Notcurses,
  Plane
} = require('..')

const nc = new Notcurses({ uncaught: true })

nc.inputStart(event => {
  // restore terminal and exit
  nc.destroy()
})

const plane = new Plane(nc, {
  y: 1,
  x: 4,
  rows: 5,
  cols: 20
})

plane.putstr('Hello World!', 2, 2)
plane.perimeterRounded()

nc.render()
