module OuterModule {
  module M1 {
    private config const foo = 10;

  }

  writeln(M1.foo);
  // Ensures that the private module level constant foo is not visible
  // via explicit naming.
}
