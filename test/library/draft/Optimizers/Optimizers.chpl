/*
  Module storing some strategies for optimization.  Originally part of CrayAI's
  HPO implementation
 */
module Optimizers {
  private use Random;
  private use List;
  private use IO;
  private use Helpers;

  /* number of attempts to make before returning the best */
  config param numIters: int = 10;

  /* A type to wrap arguments used to optimize your function */
  record optimizableArg: writeSerializable {
    /* The name of your argument */
    var name: string;

    @chpldoc.nodoc
    var intValue: int;
    @chpldoc.nodoc
    var intBounds: 2*int;
    // TODO: allow more than just (low, high) bounds - allow defined sets of
    // values
    // TODO: allow conditions

    /* Initialize the argument with its name and starting value */
    proc init(name: string, val: int) {
      this.name = name;
      this.intValue = val;

      init this;
    }

    /* Initialize the argument with its name, starting value, and
       minimum/maximum possible values */
    proc init(name: string, val: int, valBounds: 2*int) {
      this.name = name;
      this.intValue = val;
      this.intBounds = valBounds;

      init this;
    }

    proc serialize(writer: fileWriter(?), ref serializer) throws {
      // TODO: generalize for more argument types
      var ser = serializer.startRecord(writer, "optimizableArg", 1);
      ser.writeField(this.name, this.intValue);
      ser.endRecord();
    }

    /* The current value the argument is using */
    proc value {
      // TODO: update when support multiple argument types
      return intValue;
    }

    /* The lowest possible value the argument could be */
    proc lowerBound {
      // TODO: update when support multiple argument types
      // TODO: update when support bound sets instead of just ranges
      return intBounds(0);
    }

    /* The highest possible value the argument could be */
    proc upperBound {
      // TODO: update when support multiple argument types
      // TODO: update when support bound sets instead of just ranges
      return intBounds(1);
    }
  }

  /* Optimize using random sampling, returning the combination that leads to the
     best result.

     .. note::
       Assumes the order of arguments in optimizableArgs is correct.  Cannot use
       the name of the argument in the call (yet).

       Assumes all `optimizableArgs` are used before all `args`.  Cannot
       intermingle the two categories of arguments.

     :arg func: The function to optimize.  This function is assumed to return a
                result that is greater than or equal to zero.  Closer to zero
                is assumed to be better.
     :arg optimizableArgs: list of each optimizable aspect
     :arg args: additional arguments for the function to use.  Expected to be
                provided in a tuple

     :returns: a map of the name of each optimizable aspect to the value found
               for it that leads to the best result for the optimized function.
   */
  proc randomOptimize(func, optimizableArgs: list(optimizableArg),
                      args=none)
    : list(optimizableArg) throws {

    var basePoint = new Point(optimizableArgs);

    var points: [0..<numIters] Point;
    for i in points.indices {
      points[i] = generateRandom(basePoint);
    }

    // Call the provided function with each possible set of values
    // TODO: pass in the additional args as well
    evaluate(func, points);

    var bestVal = max(real);
    var bestIndex = -1;

    for i in points.indices {
      ref p = points[i];
      if p.status == Status.completed && p.fom < bestVal {
        bestVal = p.fom;
        bestIndex = i;
      }
    }

    var best: list(optimizableArg);

    if bestIndex >= 0 {
      for arg in points[bestIndex].parameters {
        best.pushBack(arg);
      }
    } else {
      // TODO: throw if nothing completed
    }

    return best;
  }

  private var rngInt = new randomStream(int);

  // Figures out a set of values to use
  private proc generateRandom(basePoint: Point): Point {
    var point = new Point();

    // TODO: generalize for more value types than just ints
    // TODO: handle not actually having bounds set
    for arg in basePoint.parameters {
      var newArg = new optimizableArg(arg.name,
                                      rngInt.next(arg.lowerBound,
                                                  arg.upperBound));
      point.parameters.pushBack(newArg);
    }

    return point;
  }

  // This is expected to be generally helpful for the various optimizers
  // Run the function with each of the possible combinations to try, updating
  // to indicate the status of running with that combination and the result
  private proc evaluate(func, ref points: [] Point) {
    param numOptArgs = func.argTypes.size;
    forall i in points {
      // TODO: bundle up the arguments in a way that's understandable
      // David thinks we can't use named arguments in FCPs yet, so need to be
      // careful about argument ordering
      select numOptArgs {
          when 1 {
            i.fom = func(i.parameters[0].value);
            i.status = Status.completed;
          }
          when 2 {
            i.fom = func(i.parameters[0].value, i.parameters[1].value);
            i.status = Status.completed;
          }
          when 3 {
            i.fom = func(i.parameters[0].value, i.parameters[1].value,
                         i.parameters[2].value);
            i.status = Status.completed;
          }
          when 4 {
            i.fom = func(i.parameters[0].value, i.parameters[1].value,
                         i.parameters[2].value, i.parameters[3].value);
            i.status = Status.completed;
          }
          when 5 {
            i.fom = func(i.parameters[0].value, i.parameters[1].value,
                         i.parameters[2].value, i.parameters[3].value,
                         i.parameters[4].value);
            i.status = Status.completed;
          }
          when 0 {
            // Error condition
          }
          otherwise {
            // Error condition
          }
      }
    }
  }

  private module Helpers {
    private use Optimizers;
    private use List;

    /* Possible statuses of an evaluation */
    enum Status {completed, failed, timedOut, running, unevaluated}

    record Point {
      var parameters: list(optimizableArg);
      var fom: real = 0.0;
      var status: Status = Status.unevaluated;

      proc init(l: list(optimizableArg)) {
        this.parameters = new list(optimizableArg);

        init this;

        for arg in l {
          this.parameters.pushBack(l);
        }
      }

      proc init() { }
    }
  }
}
