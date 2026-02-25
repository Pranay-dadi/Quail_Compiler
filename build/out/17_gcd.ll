; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @gcd(i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %.not14 = icmp eq i32 %1, 0
  br i1 %.not14, label %while.end, label %while.body

while.body:                                       ; preds = %entry, %while.body
  %a.016 = phi i32 [ %b.015, %while.body ], [ %0, %entry ]
  %b.015 = phi i32 [ %2, %while.body ], [ %1, %entry ]
  %2 = srem i32 %a.016, %b.015
  %.not = icmp eq i32 %2, 0
  br i1 %.not, label %while.end, label %while.body

while.end:                                        ; preds = %while.body, %entry
  %a.0.lcssa = phi i32 [ %0, %entry ], [ %b.015, %while.body ]
  ret i32 %a.0.lcssa
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @main() local_unnamed_addr #0 {
entry:
  br label %while.body.i

while.body.i:                                     ; preds = %while.body.i, %entry
  %a.016.i = phi i32 [ %b.015.i, %while.body.i ], [ 48, %entry ]
  %b.015.i = phi i32 [ %0, %while.body.i ], [ 18, %entry ]
  %0 = srem i32 %a.016.i, %b.015.i
  %.not.i = icmp eq i32 %0, 0
  br i1 %.not.i, label %gcd.exit, label %while.body.i

gcd.exit:                                         ; preds = %while.body.i
  ret i32 %b.015.i
}

attributes #0 = { nofree norecurse nosync nounwind readnone }
