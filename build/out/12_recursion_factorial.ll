; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: nofree nosync nounwind readnone
define i32 @factorial(i32 %0) local_unnamed_addr #0 {
entry:
  %1 = icmp slt i32 %0, 2
  br i1 %1, label %common.ret, label %ifcont

common.ret:                                       ; preds = %ifcont, %entry
  %accumulator.tr.lcssa = phi i32 [ 1, %entry ], [ %3, %ifcont ]
  ret i32 %accumulator.tr.lcssa

ifcont:                                           ; preds = %entry, %ifcont
  %.tr7 = phi i32 [ %2, %ifcont ], [ %0, %entry ]
  %accumulator.tr6 = phi i32 [ %3, %ifcont ], [ 1, %entry ]
  %2 = add nsw i32 %.tr7, -1
  %3 = mul i32 %.tr7, %accumulator.tr6
  %4 = icmp ult i32 %.tr7, 3
  br i1 %4, label %common.ret, label %ifcont
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @main() local_unnamed_addr #1 {
entry:
  ret i32 5040
}

attributes #0 = { nofree nosync nounwind readnone }
attributes #1 = { nofree norecurse nosync nounwind readnone }
