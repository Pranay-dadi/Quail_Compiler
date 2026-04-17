; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: nofree nosync nounwind readnone
define i32 @factorial(i32 %0) local_unnamed_addr #0 {
entry:
  %le6 = icmp slt i32 %0, 2
  br i1 %le6, label %common.ret, label %ifcont

common.ret:                                       ; preds = %ifcont, %entry
  %accumulator.tr.lcssa = phi i32 [ 1, %entry ], [ %mul, %ifcont ]
  ret i32 %accumulator.tr.lcssa

ifcont:                                           ; preds = %entry, %ifcont
  %.tr8 = phi i32 [ %sub, %ifcont ], [ %0, %entry ]
  %accumulator.tr7 = phi i32 [ %mul, %ifcont ], [ 1, %entry ]
  %sub = add nsw i32 %.tr8, -1
  %mul = mul i32 %.tr8, %accumulator.tr7
  %le = icmp ult i32 %.tr8, 3
  br i1 %le, label %common.ret, label %ifcont
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @main() local_unnamed_addr #1 {
entry:
  ret i32 5040
}

attributes #0 = { nofree nosync nounwind readnone }
attributes #1 = { nofree norecurse nosync nounwind readnone }
