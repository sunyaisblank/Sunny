/**
 * @name Modular arithmetic used as array index without bounds check
 * @description Uses of % 12, % 7, or similar modular reduction on values
 *              used as array indices, where the caller's contract does not
 *              specify modular semantics.
 * @kind problem
 * @problem.severity warning
 * @id sunny/modular-index-masking
 * @tags correctness
 *       arithmetic
 */

import cpp

from RemExpr modExpr, ArrayExpr arrayAccess
where
  // The modular expression is used as an array index
  arrayAccess.getArrayOffset() = modExpr
  // The divisor is a known musical constant (12, 7, etc.)
  and modExpr.getRightOperand().getValue().toInt() in [7, 12]
  // Exclude test files
  and not modExpr.getFile().getRelativePath().matches("%Test%")
select modExpr, "Modular arithmetic (% " + modExpr.getRightOperand().getValue() + ") used as array index — verify this masks rather than bounds-checks."
