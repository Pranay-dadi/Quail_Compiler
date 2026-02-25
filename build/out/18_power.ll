; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @power(i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %2 = icmp sgt i32 %1, 0
  br i1 %2, label %while.body, label %while.end

while.body:                                       ; preds = %entry, %while.body
  %result.09 = phi i32 [ %3, %while.body ], [ 1, %entry ]
  %exp.08 = phi i32 [ %4, %while.body ], [ %1, %entry ]
  %3 = mul i32 %result.09, %0
  %4 = add nsw i32 %exp.08, -1
  %5 = icmp ugt i32 %exp.08, 1
  br i1 %5, label %while.body, label %while.end

while.end:                                        ; preds = %while.body, %entry
  %result.0.lcssa = phi i32 [ 1, %entry ], [ %3, %while.body ]
  ret i32 %result.0.lcssa
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @main() local_unnamed_addr #0 {
entry:
  ret i32 256
}

attributes #0 = { nofree norecurse nosync nounwind readnone }
