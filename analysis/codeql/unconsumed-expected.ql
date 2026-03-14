/**
 * @name Unconsumed std::expected return value
 * @description A call to a function returning std::expected<T, ErrorCode> where the
 *              return value is discarded, or .value() is called without a prior
 *              .has_value() check. This creates an unguarded throw on the error path.
 * @kind problem
 * @problem.severity warning
 * @id sunny/unconsumed-expected
 * @tags correctness
 *       reliability
 */

import cpp

/**
 * Holds if `f` returns a type whose name contains "expected".
 */
predicate returnsExpected(Function f) {
  f.getType().getUnspecifiedType().getName().matches("%expected%")
  or
  f.getType().getUnspecifiedType().(ClassTemplateInstantiation).getTemplateArgument(0).toString() != "" and
  f.getType().getUnspecifiedType().getName().matches("%expected%")
}

/**
 * A call expression whose return type involves std::expected.
 */
class ExpectedCall extends FunctionCall {
  ExpectedCall() {
    returnsExpected(this.getTarget())
  }
}

from ExpectedCall call
where
  // The call result is used as an expression statement (discarded)
  call.getParent() instanceof ExprStmt
  // Exclude test files
  and not call.getFile().getRelativePath().matches("%Test%")
  and not call.getFile().getRelativePath().matches("%test%")
select call, "Return value of " + call.getTarget().getName() + " (returning std::expected) is discarded without checking for error."
