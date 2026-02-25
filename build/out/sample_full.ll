; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @fibonacci(i32 %0) local_unnamed_addr #0 {
entry:
  %1 = icmp slt i32 %0, 2
  br i1 %1, label %common.ret, label %while.body

common.ret:                                       ; preds = %while.body, %entry
  %common.ret.op = phi i32 [ %0, %entry ], [ %3, %while.body ]
  ret i32 %common.ret.op

while.body:                                       ; preds = %entry, %while.body
  %storemerge15 = phi i32 [ %4, %while.body ], [ 2, %entry ]
  %b61214 = phi i32 [ %2, %while.body ], [ 0, %entry ]
  %2 = phi i32 [ %3, %while.body ], [ 1, %entry ]
  %3 = add i32 %2, %b61214
  %4 = add i32 %storemerge15, 1
  %.not = icmp sgt i32 %4, %0
  br i1 %.not, label %common.ret, label %while.body
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @sum_array(i32 %0) local_unnamed_addr #0 {
entry:
  %arr = alloca [8 x i32], align 4
  %1 = icmp sgt i32 %0, 0
  br i1 %1, label %while.body, label %while.end7

while.body:                                       ; preds = %entry, %while.body
  %k.018 = phi i32 [ %4, %while.body ], [ 0, %entry ]
  %2 = shl nuw i32 %k.018, 1
  %3 = zext i32 %k.018 to i64
  %arr.gep = getelementptr [8 x i32], [8 x i32]* %arr, i64 0, i64 %3
  store i32 %2, i32* %arr.gep, align 4
  %4 = add nuw nsw i32 %k.018, 1
  %5 = icmp slt i32 %4, %0
  br i1 %5, label %while.body, label %while.end

while.end:                                        ; preds = %while.body
  br i1 %1, label %while.body6, label %while.end7

while.body6:                                      ; preds = %while.end, %while.body6
  %storemerge19 = phi i32 [ %9, %while.body6 ], [ 0, %while.end ]
  %6 = phi i32 [ %8, %while.body6 ], [ 0, %while.end ]
  %7 = zext i32 %storemerge19 to i64
  %arr.gep12 = getelementptr [8 x i32], [8 x i32]* %arr, i64 0, i64 %7
  %arr.load = load i32, i32* %arr.gep12, align 4
  %8 = add i32 %arr.load, %6
  %9 = add nuw nsw i32 %storemerge19, 1
  %10 = icmp slt i32 %9, %0
  br i1 %10, label %while.body6, label %while.end7

while.end7:                                       ; preds = %while.body6, %entry, %while.end
  %.lcssa = phi i32 [ 0, %while.end ], [ 0, %entry ], [ %8, %while.body6 ]
  ret i32 %.lcssa
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @factorial(i32 %0) local_unnamed_addr #0 {
entry:
  %.not11 = icmp slt i32 %0, 1
  br i1 %.not11, label %for.end.thread, label %for.body

for.body:                                         ; preds = %entry, %for.body
  %i.013 = phi i32 [ %2, %for.body ], [ 1, %entry ]
  %result.012 = phi i32 [ %1, %for.body ], [ 1, %entry ]
  %1 = mul i32 %i.013, %result.012
  %2 = add i32 %i.013, 1
  %.not = icmp sgt i32 %2, %0
  br i1 %.not, label %for.end, label %for.body

for.end:                                          ; preds = %for.body
  %3 = icmp slt i32 %1, 1000
  br i1 %3, label %for.end.thread, label %4

for.end.thread:                                   ; preds = %entry, %for.end
  %result.0.lcssa15 = phi i32 [ %1, %for.end ], [ 1, %entry ]
  br label %4

4:                                                ; preds = %for.end, %for.end.thread
  %5 = phi i32 [ %result.0.lcssa15, %for.end.thread ], [ 1000, %for.end ]
  ret i32 %5
}

; Function Attrs: nofree nosync nounwind readnone
define i32 @main() local_unnamed_addr #1 {
entry:
  ret i32 255
}

attributes #0 = { nofree norecurse nosync nounwind readnone }
attributes #1 = { nofree nosync nounwind readnone }
