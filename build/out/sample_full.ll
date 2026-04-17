; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @fibonacci(i32 %0) local_unnamed_addr #0 {
entry:
  %le = icmp slt i32 %0, 2
  br i1 %le, label %common.ret, label %while.body

common.ret:                                       ; preds = %while.body, %entry
  %common.ret.op = phi i32 [ %0, %entry ], [ %add, %while.body ]
  ret i32 %common.ret.op

while.body:                                       ; preds = %entry, %while.body
  %storemerge19 = phi i32 [ %inc, %while.body ], [ 2, %entry ]
  %b71418 = phi i32 [ %add1517, %while.body ], [ 0, %entry ]
  %add1517 = phi i32 [ %add, %while.body ], [ 1, %entry ]
  %add = add i32 %add1517, %b71418
  %inc = add i32 %storemerge19, 1
  %le5.not = icmp sgt i32 %inc, %0
  br i1 %le5.not, label %common.ret, label %while.body
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @sum_array(i32 %0) local_unnamed_addr #0 {
entry:
  %arr = alloca [8 x i32], align 4
  %lt22 = icmp sgt i32 %0, 0
  br i1 %lt22, label %while.body, label %while.end8

while.body:                                       ; preds = %entry, %while.body
  %k.023 = phi i32 [ %inc, %while.body ], [ 0, %entry ]
  %mul = shl nuw i32 %k.023, 1
  %1 = zext i32 %k.023 to i64
  %arr.gep = getelementptr [8 x i32], [8 x i32]* %arr, i64 0, i64 %1
  store i32 %mul, i32* %arr.gep, align 4
  %inc = add nuw nsw i32 %k.023, 1
  %lt = icmp slt i32 %inc, %0
  br i1 %lt, label %while.body, label %while.end

while.end:                                        ; preds = %while.body
  br i1 %lt22, label %while.body7, label %while.end8

while.body7:                                      ; preds = %while.end, %while.body7
  %storemerge27 = phi i32 [ %inc16, %while.body7 ], [ 0, %while.end ]
  %add2426 = phi i32 [ %add, %while.body7 ], [ 0, %while.end ]
  %2 = zext i32 %storemerge27 to i64
  %arr.gep14 = getelementptr [8 x i32], [8 x i32]* %arr, i64 0, i64 %2
  %arr.load = load i32, i32* %arr.gep14, align 4
  %add = add i32 %arr.load, %add2426
  %inc16 = add nuw nsw i32 %storemerge27, 1
  %lt11 = icmp slt i32 %inc16, %0
  br i1 %lt11, label %while.body7, label %while.end8

while.end8:                                       ; preds = %while.body7, %entry, %while.end
  %add24.lcssa = phi i32 [ 0, %while.end ], [ 0, %entry ], [ %add, %while.body7 ]
  ret i32 %add24.lcssa
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @factorial(i32 %0) local_unnamed_addr #0 {
entry:
  %le.not12 = icmp slt i32 %0, 1
  br i1 %le.not12, label %for.end.thread, label %for.body

for.body:                                         ; preds = %entry, %for.body
  %i.014 = phi i32 [ %inc, %for.body ], [ 1, %entry ]
  %result.013 = phi i32 [ %mul, %for.body ], [ 1, %entry ]
  %mul = mul i32 %i.014, %result.013
  %inc = add i32 %i.014, 1
  %le.not = icmp sgt i32 %inc, %0
  br i1 %le.not, label %for.end, label %for.body

for.end:                                          ; preds = %for.body
  %1 = icmp slt i32 %mul, 1000
  br i1 %1, label %for.end.thread, label %2

for.end.thread:                                   ; preds = %entry, %for.end
  %result.0.lcssa16 = phi i32 [ %mul, %for.end ], [ 1, %entry ]
  br label %2

2:                                                ; preds = %for.end, %for.end.thread
  %3 = phi i32 [ %result.0.lcssa16, %for.end.thread ], [ 1000, %for.end ]
  ret i32 %3
}

; Function Attrs: nofree nosync nounwind readnone
define i32 @main() local_unnamed_addr #1 {
entry:
  ret i32 255
}

attributes #0 = { nofree norecurse nosync nounwind readnone }
attributes #1 = { nofree nosync nounwind readnone }
