/**
 * @name Unconsumed std::expected return value
 * @description An expression statement discards a std::expected value returned by
 *              a function. References returned by assignment retain the result;
 *              unchecked .value() access requires separate control-flow analysis.
 * @kind problem
 * @problem.severity warning
 * @id sunny/unconsumed-expected
 * @tags correctness
 *       reliability
 */

import cpp

/**
 * Holds if `f` returns a std::expected value, including through a type alias.
 * The class-template cast excludes pointer/reference return types, including
 * std::expected::operator=, whose result already resides in the assigned object.
 */
predicate returnsExpected(Function f) {
  f.getType()
      .getUnspecifiedType()
      .(ClassTemplateInstantiation)
      .getTemplate()
      .hasQualifiedName("std", "", "expected")
}

/**
 * A call expression returning a std::expected value.
 */
class ExpectedCall extends FunctionCall {
  ExpectedCall() { returnsExpected(this.getTarget()) }
}

from ExpectedCall call
where
  // The call result is used as an expression statement (discarded)
  call.getParent() instanceof ExprStmt and
  // Exclude test files
  not call.getFile().getRelativePath().matches("%Test%") and
  not call.getFile().getRelativePath().matches("%test%")
select call,
  "Return value of " + call.getTarget().getName() +
    " (returning std::expected) is discarded without checking for error."
