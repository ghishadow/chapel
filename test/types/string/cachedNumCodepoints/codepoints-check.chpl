var s = "my string";
var a = s;
var d = string.createWithBorrowedBuffer(s);
writeln(d.numCodepoints);
writeln(d.size);
writeln(d.cachedNumCodepoints);
writeln(d.count("s"));
writeln(a.numCodepoints);
writeln(a.size);
writeln(a.cachedNumCodepoints);
writeln(a.count("s"));