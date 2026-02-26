; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @factorial(i32 %0) local_unnamed_addr #0 {
entry:
  %le.not10 = icmp slt i32 %0, 1
  br i1 %le.not10, label %while.end, label %while.body

while.body:                                       ; preds = %entry, %while.body
  %i.012 = phi i32 [ %inc, %while.body ], [ 1, %entry ]
  %result.011 = phi i32 [ %mul, %while.body ], [ 1, %entry ]
  %mul = mul i32 %i.012, %result.011
  %inc = add i32 %i.012, 1
  %le.not = icmp sgt i32 %inc, %0
  br i1 %le.not, label %while.end, label %while.body

while.end:                                        ; preds = %while.body, %entry
  %result.0.lcssa = phi i32 [ 1, %entry ], [ %mul, %while.body ]
  ret i32 %result.0.lcssa
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @sum_array(i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %lt10 = icmp sgt i32 %1, 0
  br i1 %lt10, label %while.body.preheader, label %while.end

while.body.preheader:                             ; preds = %entry
  %2 = add nsw i32 %1, -1
  %3 = zext i32 %2 to i33
  %4 = add nsw i32 %1, -2
  %5 = zext i32 %4 to i33
  %6 = mul i33 %3, %5
  %7 = lshr i33 %6, 1
  %8 = trunc i33 %7 to i32
  %9 = add i32 %8, %1
  %10 = add i32 %9, -1
  br label %while.end

while.end:                                        ; preds = %while.body.preheader, %entry
  %total.0.lcssa = phi i32 [ 0, %entry ], [ %10, %while.body.preheader ]
  ret i32 %total.0.lcssa
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @main() local_unnamed_addr #0 {
entry:
  ret i32 100
}

attributes #0 = { nofree norecurse nosync nounwind readnone }
