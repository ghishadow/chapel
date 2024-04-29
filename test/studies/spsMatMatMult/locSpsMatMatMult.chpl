use LayoutCS, Random;

enum layout { csr, csc };

config const n = 10,
             density = 0.05,
             seed = 0,
             skipDense = false;

var rands = if seed == 0 then new randomStream(real)
                         else new randomStream(real, seed);

// print library-selected seed, for reproducibility
if seed == 0 then
  writeln("Using seed: ", rands.seed);


const Dom = {1..n, 1..n};

const AD = randSparseMatrix(Dom, density, layout.csc),
      BD = randSparseMatrix(Dom, density, layout.csr);

var A: [AD] int = 1,
    B: [BD] int = 1;

writeSparseMatrix("A is:", A);
writeSparseMatrix("B is:", B);

const CS = SparseMatMatMult(A, B);

writeSparseMatrix("C (sparsely computed) is:", CS);

if !skipDense {
  const CD = DenseMatMatMult(A, B);
  writeSparseMatrix("C (densely computed) is: ", CD);
}


proc SparseMatMatMult(A: [?AD], B: [?BD]) {
  use List;

  var nnzs: list(2*int),
      vals: list(int);

  for ac_br in AD.colRange {
    for ai in AD.startIdx[ac_br]..<AD.startIdx[ac_br+1] {
      const ar = AD.idx[ai];

      for bi in BD.startIdx[ac_br]..<BD.startIdx[ac_br+1] {
        const bc = BD.idx[bi];

        nnzs.pushBack((ar,bc));
        vals.pushBack(A.data[ai]*B.data[bi]);
      }
    }
  }

  return makeSparseMat(nnzs, vals);
}


proc DenseMatMatMult(A, B) {
  use List;

  var nnzs: list(2*int),
      vals: list(int);

  for i in 1..n {
    for k in 1..n {
      for j in 1..n {
        var prod = A[i,k] * B[k,j];
        if prod != 0 {
          nnzs.pushBack((i,j));
          vals.pushBack(prod);
        }
      }
    }
  }

  var C = makeSparseMat(nnzs, vals);
  
  writeSparseMatrix("C is:", C);
}


proc randSparseMatrix(Dom, density, param lay) {
  var SD: sparse subdomain(Dom) dmapped CS(compressRows=(lay==layout.csr));

  for (i,j) in Dom do
    if rands.next() <= density then
      SD += (i,j);

  return SD;
}


proc writeSparseMatrix(msg, Arr) {
  const ref SparseDom = Arr.domain,
            DenseDom = SparseDom.parentDom;

  writeln(msg);

  for r in DenseDom.dim(0) {
    for c in DenseDom.dim(1) {
      write(Arr[r,c], " ");
    }
    writeln();
  }
  writeln();    
}




proc makeSparseMat(nnzs, vals) {
  var CDom: sparse subdomain(A.domain.parentDom);
  for ij in nnzs do
    CDom += ij;

  var C: [CDom] int;
  for (ij, c) in zip(nnzs, vals) do
    C[ij] += c;
  return C;
}
