class C {
  iter these() {
    for i in 1..10 do
      yield i;
  }

  iter these(param tag: iterKind) where tag == iterKind.leader {
    yield 1..10;
  }

  iter these(param tag: iterKind, followThis) where tag == iterKind.follower {
    for i in followThis do
      yield i;
  }
}

class D {
  // BLC: Really, I'd prefer not to define this one either, but parNoSolo.chpl
  // already captures this case
  iter these() {
    for i in 1..10 do
      yield i;
  }
  //

  iter these(param tag: iterKind, followThis) where tag == iterKind.follower {
    for i in followThis do
      yield -i;
  }
}

var ownMyC = new owned C();
var myC = ownMyC.borrow();
var ownMyD = new owned D();
var myD = ownMyD.borrow();

forall (i,j) in zip(myC, myD) {
  writeln("(i,j) = ", (i,j));
}
