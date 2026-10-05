/**
 * @name Beat construction with constant zero denominator
 * @description Construction of sunny::core::Beat with a constant zero denominator.
 *              Dynamic denominators require separate validation.
 * @kind problem
 * @problem.severity error
 * @id sunny/beat-zero-denominator
 * @tags correctness
 *       arithmetic
 */

import cpp

from ConstructorCall call
where
  call.getTarget().getDeclaringType().hasQualifiedName("sunny::core", "", "Beat") and
  not call.getFile().getRelativePath().matches("%Test%") and
  not call.getFile().getRelativePath().matches("%test%") and
  call.getNumberOfArguments() = 2 and
  call.getArgument(1).getValue() = "0"
select call, "Beat constructed with zero denominator."
