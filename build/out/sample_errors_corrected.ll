; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @factorial(i32 %0) local_unnamed_addr #0 {
entry:
  %.not9 = icmp slt i32 %0, 1
  br i1 %.not9, label %while.end, label %while.body

while.body:                                       ; preds = %entry, %while.body
  %i.011 = phi i32 [ %2, %while.body ], [ 1, %entry ]
  %result.010 = phi i32 [ %1, %while.body ], [ 1, %entry ]
  %1 = mul i32 %i.011, %result.010
  %2 = add i32 %i.011, 1
  %.not = icmp sgt i32 %2, %0
  br i1 %.not, label %while.end, label %while.body

while.end:                                        ; preds = %while.body, %entry
  %result.0.lcssa = phi i32 [ 1, %entry ], [ %1, %while.body ]
  ret i32 %result.0.lcssa
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @sum_array(i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %2 = icmp sgt i32 %1, 0
  br i1 %2, label %while.body.preheader, label %while.end

while.body.preheader:                             ; preds = %entry
  %3 = add nsw i32 %1, -1
  %4 = zext i32 %3 to i33
  %5 = add nsw i32 %1, -2
  %6 = zext i32 %5 to i33
  %7 = mul i33 %4, %6
  %8 = lshr i33 %7, 1
  %9 = trunc i33 %8 to i32
  %10 = add i32 %9, %1
  %11 = add i32 %10, -1
  br label %while.end

while.end:                                        ; preds = %while.body.preheader, %entry
  %total.0.lcssa = phi i32 [ 0, %entry ], [ %11, %while.body.preheader ]
  ret i32 %total.0.lcssa
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @main() local_unnamed_addr #0 {
entry:
  ret i32 100
}

attributes #0 = { nofree norecurse nosync nounwind readnone }
