; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @power(i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %gt8 = icmp sgt i32 %1, 0
  br i1 %gt8, label %while.body, label %while.end

while.body:                                       ; preds = %entry, %while.body
  %result.010 = phi i32 [ %mul, %while.body ], [ 1, %entry ]
  %exp.09 = phi i32 [ %sub, %while.body ], [ %1, %entry ]
  %mul = mul i32 %result.010, %0
  %sub = add nsw i32 %exp.09, -1
  %gt = icmp ugt i32 %exp.09, 1
  br i1 %gt, label %while.body, label %while.end

while.end:                                        ; preds = %while.body, %entry
  %result.0.lcssa = phi i32 [ 1, %entry ], [ %mul, %while.body ]
  ret i32 %result.0.lcssa
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @main() local_unnamed_addr #0 {
entry:
  ret i32 256
}

attributes #0 = { nofree norecurse nosync nounwind readnone }
