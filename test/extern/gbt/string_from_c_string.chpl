extern proc returns_c_string(): c_string;

writeln("Should be returned_c_string");
writeln(createStringWithNewBuffer(returns_c_string()));
