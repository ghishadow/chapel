class Foo {
  param p;

  proc init() {
    p = 2;

  }
}

class Bar : Foo {
  var x: int;

  proc init(xVal) {
    x = xVal;
    p = 6;

  }
}

var bar = new Bar(4);
var b = bar.borrow();
writeln(b.type: string);
writeln(b);
