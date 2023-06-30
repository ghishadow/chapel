import BlockDist.Block,
       Collectives.barrier;

use CommDiagnostics;
config param runCommDiag = false;

config const xLen = 2.0,    // length of the grid in x
             yLen = 2.0,    // length of the grid in y
             nx = 31,       // number of grid points in x
             ny = 31,       // number of grid points in y
             nt = 50,       // number of time steps
             sigma = 0.25,  // stability parameter
             nu = 0.05;     // viscosity

const dx : real = xLen / (nx - 1),       // grid spacing in x
      dy : real = yLen / (ny - 1),       // grid spacing in y
      dt : real = sigma * dx * dy / nu;  // time step size

// define block distributed array
const indices = {0..<nx, 0..<ny},
      indicesInner = indices.expand(-1),
      INDICES = indices dmapped Block(indicesInner);
var u: [INDICES] real;

// apply initial conditions
u = 1.0;
u[
  (0.5 / dx):int..<(1.0 / dx + 1):int,
  (0.5 / dy):int..<(1.0 / dy + 1):int
] = 2;

// number of tasks per dimension based on Block distributions decomposition
const tidXMax = u.targetLocales().dim(0).high,
      tidYMax = u.targetLocales().dim(1).high;

// barrier for one-task-per-locale
var b = new barrier(u.targetLocales().size);

enum Edge { N, E, S, W }
class localArraySet {
  // which of the global indices does this locale own
  // used to index into the global array.
  var globalIndices: domain(2) = {0..0, 0..0};

  // indices over which local arrays are defined
  // same as 'globalIndices' set with buffers along each edge
  var indices: domain(2) = {0..0, 0..0};

  // indices over which the kernel is executed
  // can differ from 'globalIndices' for locales along the border of th
  //  global domain
  var compIndices: domain(2) = {0..0, 0..0};

  var a: [indices] real;
  var b: [indices] real;

  proc init() {
    this.globalIndices = {0..0, 0..0};
    this.indices = {0..0, 0..0};
    this.compIndices = {0..0, 0..0};
  }

  proc init(globalIndices: domain(2), globalInner: domain(2)) {
    this.globalIndices = globalIndices;
    this.indices = globalIndices.expand(1);
    this.compIndices = globalIndices[globalInner];
  }

  proc ref copyInitialConditions(const ref u: [] real) {
    this.a = 1.0;
    this.a[this.globalIndices] = u[globalIndices];
    this.b = this.a;
  }

  proc fillBuffer(edge: Edge, values: [] real) {
    select edge {
      when Edge.N do this.a[this.indices.dim(0).low, ..] = values;
      when Edge.S do this.a[this.indices.dim(0).high, ..] = values;
      when Edge.E do this.a[.., this.indices.dim(1).high] = values;
      when Edge.W do this.a[.., this.indices.dim(1).low] = values;
    }
  }

  proc getEdge(edge: Edge) {
    select edge {
      when Edge.N do return this.a[this.indices.dim(0).low+1, ..];
      when Edge.S do return this.a[this.indices.dim(0).high-1, ..];
      when Edge.E do return this.a[.., this.indices.dim(1).high-1];
      when Edge.W do return this.a[.., this.indices.dim(1).low+1];
    }
    return this.a[this.indices.dim(0).low, ..]; // never actually returned
  }

  proc swap() {
    this.a <=> this.b;
  }
}

// !!! TODO: put localArrays on their respective target locales

// sets of local arrays owned by each locale
var localArrays = [d in u.targetLocales().domain] new localArraySet();

proc main() {
  if runCommDiag then startVerboseComm();

  // execute the FD computation with one task per locale
  coforall (loc, (tidX, tidY)) in zip(u.targetLocales(), u.targetLocales().domain) do on loc {

    // initialize local arrays owned by this task
    localArrays[tidX, tidY] = new localArraySet(u.localSubdomain(here), indicesInner);
    localArrays[tidX, tidY].copyInitialConditions(u);

    b.barrier();

    // run the portion of the FD computation owned by this task
    work(tidX, tidY);
  }

  if runCommDiag {
    stopVerboseComm();
    printCommDiagnosticsTable();
  }

  // print final results
  const mean = (+ reduce u) / u.size,
        stdDev = sqrt((+ reduce (u - mean)**2) / u.size);

  writeln(abs(0.102424 - stdDev) < 1e-6);
  writeln(u);
}

proc work(tidX: int, tidY: int) {
  var uLocal: borrowed localArraySet = localArrays[tidX, tidY];

  // preliminarily populate ghost regions for neighboring locales
  if tidX > 0       then localArrays[tidX-1, tidY].fillBuffer(Edge.S, uLocal.getEdge(Edge.N));
  if tidX < tidXMax then localArrays[tidX+1, tidY].fillBuffer(Edge.N, uLocal.getEdge(Edge.S));
  if tidY > 0       then localArrays[tidX, tidY-1].fillBuffer(Edge.W, uLocal.getEdge(Edge.E));
  if tidY < tidYMax then localArrays[tidX, tidY+1].fillBuffer(Edge.E, uLocal.getEdge(Edge.W));

  b.barrier();

  // run FD computation
  for 1..nt {

    // writeln("(", tidX, ", ", tidY, ")\n", uLocal.a, "\n");

    // compute the FD kernel in parallel
    forall (i, j) in uLocal.compIndices {
      uLocal.b[i, j] = uLocal.a[i, j] +
              nu * dt / dy**2 *
                (uLocal.a[i-1, j] - 2 * uLocal.a[i, j] + uLocal.a[i+1, j]) +
              nu * dt / dx**2 *
                (uLocal.a[i, j-1] - 2 * uLocal.a[i, j] + uLocal.a[i, j+1]);
    }

    b.barrier();

    uLocal.swap();

    b.barrier();

    // populate ghost regions for neighboring locales
    if tidX > 0       then localArrays[tidX-1, tidY].fillBuffer(Edge.S, uLocal.getEdge(Edge.N));
    if tidX < tidXMax then localArrays[tidX+1, tidY].fillBuffer(Edge.N, uLocal.getEdge(Edge.S));
    if tidY > 0       then localArrays[tidX, tidY-1].fillBuffer(Edge.W, uLocal.getEdge(Edge.E));
    if tidY < tidYMax then localArrays[tidX, tidY+1].fillBuffer(Edge.E, uLocal.getEdge(Edge.W));

    b.barrier();
  }

  // store results in global array
  u[uLocal.globalIndices] = uLocal.a[uLocal.globalIndices];
}
