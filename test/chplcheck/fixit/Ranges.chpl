module SimpleDomainAsRange {

  @chplcheck.ignore("UnusedLoopIndex")
  for i in {1..10} {}

  @chplcheck.ignore("UnusedLoopIndex")
  for i in {1..<10} {}

  @chplcheck.ignore("UnusedLoopIndex")
  for i in {1..#10} {}

  @chplcheck.ignore("UnusedLoopIndex")
  for i in {1..#10 by 2} {}

  @chplcheck.ignore("UnusedLoopIndex")
  for i in {1..#10 align 2} {}

  @chplcheck.ignore("UnusedLoopIndex")
  for i in {1..#10 by 2 align 2} {}

  for {1..10} {}

  for {1..<10} {}

  for {1..#10} {}

  for {1..#10 by 2} {}

  for {1..#10 align 2} {}

  for {1..#10 by 2 align 2} {}

  var A: [1..10] int;

  @chplcheck.ignore("UnusedLoopIndex")
  forall i in {1..10} with (ref A) {

  }

  @chplcheck.ignore("UnusedLoopIndex")
  forall i in {1..<10} with (ref A) {

  }

  @chplcheck.ignore("UnusedLoopIndex")
  forall i in {1..#10} with (ref A) {

  }

  @chplcheck.ignore("UnusedLoopIndex")
  forall i in {1..#10 by 2} with (ref A) {

  }

  @chplcheck.ignore("UnusedLoopIndex")
  forall i in {1..#10 align 2} with (ref A) {

  }

  @chplcheck.ignore("UnusedLoopIndex")
  forall i in {1..#10 by 2 align 2} with (ref A) {

  }

  forall {1..10} with (ref A) {

  }

  forall {1..<10} with (ref A) {

  }

  forall {1..#10} with (ref A) {

  }

  forall {1..#10 by 2} with (ref A) {

  }

  forall {1..#10 align 2} with (ref A) {

  }

  forall {1..#10 by 2 align 2} with (ref A) {

  }
}
