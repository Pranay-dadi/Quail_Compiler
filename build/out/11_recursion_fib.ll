; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: nofree nosync nounwind readnone
define i32 @fib(i32 %0) local_unnamed_addr #0 {
entry:
  %1 = icmp slt i32 %0, 2
  br i1 %1, label %common.ret, label %ifcont

common.ret:                                       ; preds = %ifcont, %entry
  %accumulator.tr.lcssa = phi i32 [ 0, %entry ], [ %5, %ifcont ]
  %.tr.lcssa = phi i32 [ %0, %entry ], [ %4, %ifcont ]
  %accumulator.ret.tr = add i32 %.tr.lcssa, %accumulator.tr.lcssa
  ret i32 %accumulator.ret.tr

ifcont:                                           ; preds = %entry, %ifcont
  %.tr9 = phi i32 [ %4, %ifcont ], [ %0, %entry ]
  %accumulator.tr8 = phi i32 [ %5, %ifcont ], [ 0, %entry ]
  %2 = add nsw i32 %.tr9, -1
  %3 = tail call i32 @fib(i32 %2)
  %4 = add nsw i32 %.tr9, -2
  %5 = add i32 %3, %accumulator.tr8
  %6 = icmp ult i32 %.tr9, 4
  br i1 %6, label %common.ret, label %ifcont
}

; Function Attrs: nofree nosync nounwind readnone
define i32 @main() local_unnamed_addr #0 {
entry:
  %0 = tail call i32 @fib(i32 10)
  ret i32 %0
}

attributes #0 = { nofree nosync nounwind readnone }
