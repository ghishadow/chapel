config const n = 8;
var d: domain(1) = {1..n};
var sd1: sparse subdomain(d) = {1..n};
var sd2: sparse subdomain(d) = {1..n};

forall (i,j,k) in zip(sd1,sd2,sd1) do writeln(i,j,k);
