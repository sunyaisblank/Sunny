/**
 * @name Beat construction with potentially zero denominator
 * @description Construction of Beat{numerator, denominator} where the denominator
 *              could be zero.
 * @kind problem
 * @problem.severity error
 * @id sunny/beat-zero-denominator
 * @tags correctness
 *       arithmetic
 */

import cpp

from AggregateLiteral al
where
  al.getType().getName().matches("%Beat%")
  and not al.getFile().getRelativePath().matches("%Test%")
  and al.getNumChild() >= 2
  and al.getChild(1).getValue() = "0"
select al, "Beat constructed with zero denominator."
