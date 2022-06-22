use BigInteger;

var s = new bigint();

const a = new bigint("90000000000000000000");
const b = new bigint("40000000000000000000");
writeln(a % b);
s.mod(a, b);
writeln(s);

const c = new bigint("90000000000000000000");
const d = new bigint("-40000000000000000000");
writeln(c % d);
s.mod(c, d);
writeln(s);

const e = new bigint("-90000000000000000000");
const f = new bigint("40000000000000000000");
writeln(e % f);
s.mod(e, f);
writeln(s);

const g = new bigint("-90000000000000000000");
const h = new bigint("-40000000000000000000");
writeln(g % h);
s.mod(g, h);
writeln(s);
