; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: nofree nosync nounwind readnone
define i32 @fib(i32 %0) local_unnamed_addr #0 {
entry:
  %le10 = icmp slt i32 %0, 2
  br i1 %le10, label %common.ret, label %ifcont

common.ret:                                       ; preds = %ifcont, %entry
  %accumulator.tr.lcssa = phi i32 [ 0, %entry ], [ %add, %ifcont ]
  %.tr.lcssa = phi i32 [ %0, %entry ], [ %sub5, %ifcont ]
  %accumulator.ret.tr = add i32 %.tr.lcssa, %accumulator.tr.lcssa
  ret i32 %accumulator.ret.tr

ifcont:                                           ; preds = %entry, %ifcont
  %.tr12 = phi i32 [ %sub5, %ifcont ], [ %0, %entry ]
  %accumulator.tr11 = phi i32 [ %add, %ifcont ], [ 0, %entry ]
  %sub = add nsw i32 %.tr12, -1
  %calltmp = tail call i32 @fib(i32 %sub)
  %sub5 = add nsw i32 %.tr12, -2
  %add = add i32 %calltmp, %accumulator.tr11
  %le = icmp ult i32 %.tr12, 4
  br i1 %le, label %common.ret, label %ifcont
}

; Function Attrs: nofree nosync nounwind readnone
define i32 @main() local_unnamed_addr #0 {
entry:
  %calltmp = tail call i32 @fib(i32 10)
  ret i32 %calltmp
}

attributes #0 = { nofree nosync nounwind readnone }
