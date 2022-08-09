use IO;

class mything {
  var x:int;

  proc readThis(r) throws {
    r.read(x);
  }

  proc writeThis(w) throws {
    w.write(x);
  }
}

class subthing : mything {
  var y:int;

  override proc readThis(r) throws {
    x = r.read(int);
    r <~> new ioLiteral(",");
    y = r.read(int);
  }

  override proc writeThis(w) throws {
    w.write(x);
    w <~> new ioLiteral(",");
    w.write(y);
  }
}


{
  var a = new borrowed mything(1);

  writeln("Writing ", a);

  var f = openmem();
  var w = f.writer();

  w.write(a);
  w.close();

  var r = f.reader();

  var b = new borrowed mything(2);

  r.read(b);
  r.close();

  writeln("Read ", b);

  assert(a.x == b.x);
}

{
  var a = new borrowed subthing(3,4);

  writeln("Writing ", a);

  var f = openmem();
  var w = f.writer();

  w.write(a);
  w.close();

  var r = f.reader();

  var b = new borrowed subthing(5,6);

  r.read(b);
  r.close();

  writeln("Read ", b);

  assert(a.x == b.x);
  assert(a.y == b.y);
}

{
  var a = new borrowed subthing(3,4);

  writeln("Writing ", a);

  var f = openmem();
  var w = f.writer();

  w.write(a);
  w.close();

  var r = f.reader();

  var b = new borrowed subthing(5,6);
  var c:borrowed mything = b;

  r.read(c);
  r.close();

  writeln("Read ", b);

  assert(a.x == b.x);
  assert(a.y == b.y);
}


