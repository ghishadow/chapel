use IO;

class mything {
  var x:int;
  var y:int;

  proc readThis(r) throws {
    readWriteHelper(r);
  }

  proc writeThis(w) throws {
    readWriteHelper(w);
  }

  proc readWriteHelper(rw) throws {
    if rw.writing then rw.write(x); else x = rw.read(int);
    rw.readWriteLiteral(" ");
    if rw.writing then rw.write(y); else y = rw.read(int);
    rw.readWriteNewline();
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

