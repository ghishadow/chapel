// max reduce
// list
// unbounded range slice
// strided ranges

use IO, List;

enum alphabet {A, B, C, D, E, F, G, H, I, J, K, L, M,
               N, O, P, Q, R, S, T, U, V, W, X, Y, Z};

const State = readInitState();
writeln(State);

var numStacks = max reduce ([s in State] s.size/4);
writeln("numStacks = ", numStacks);

var Stacks: [1..numStacks] list(alphabet);

for i in 0..<State.size-2 by -1 {
  write(State[i]);
  for s in 1..numStacks do {
    const char = State[i][(s-1)*4 + 1];
    if (char != " ") {
      writeln(char);
      writeln(char:alphabet);
      Stacks[s].append(char:alphabet);
    }
  }
}

writeln(Stacks);

var num, src, dst: int;
while readf("move %i from %i to %i\n", num, src, dst) {
  writeln((num, src, dst));
  var TmpStack: list(alphabet);
  for i in 1..num {
    TmpStack.append(Stacks[src].pop());
  }
  for i in 1..num do
    Stacks[dst].append(TmpStack.pop());
}

for s in Stacks do
  write(s.pop());
writeln();

writeln(Stacks);


iter readInitState() {
  do {
// BUG:
    var line: string;
    readLine(line);
    yield line;
  } while (line.size > 1);
}

