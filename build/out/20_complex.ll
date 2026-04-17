; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @clamp(i32 %0, i32 %1, i32 %2) local_unnamed_addr #0 {
entry:
  %lt = icmp slt i32 %0, %1
  %gt = icmp sgt i32 %0, %2
  %. = select i1 %gt, i32 %2, i32 %0
  %common.ret.op = select i1 %lt, i32 %1, i32 %.
  ret i32 %common.ret.op
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @sumArray(i32 %0) local_unnamed_addr #1 {
entry:
  %arr = alloca [10 x i32], align 4
  %lt29 = icmp sgt i32 %0, 0
  br i1 %lt29, label %for.body, label %for.end10

for.cond7.preheader:                              ; preds = %for.body
  br i1 %lt29, label %for.body8, label %for.end10

for.body:                                         ; preds = %entry, %for.body
  %i.030 = phi i32 [ %inc, %for.body ], [ 0, %entry ]
  %mul = mul i32 %i.030, %i.030
  %1 = zext i32 %i.030 to i64
  %arr.gep = getelementptr [10 x i32], [10 x i32]* %arr, i64 0, i64 %1
  store i32 %mul, i32* %arr.gep, align 4
  %inc = add nuw nsw i32 %i.030, 1
  %lt = icmp slt i32 %inc, %0
  br i1 %lt, label %for.body, label %for.cond7.preheader

for.body8:                                        ; preds = %for.cond7.preheader, %for.body8
  %total.033 = phi i32 [ %add, %for.body8 ], [ 0, %for.cond7.preheader ]
  %i.132 = phi i32 [ %inc18, %for.body8 ], [ 0, %for.cond7.preheader ]
  %2 = zext i32 %i.132 to i64
  %arr.gep16 = getelementptr [10 x i32], [10 x i32]* %arr, i64 0, i64 %2
  %arr.load = load i32, i32* %arr.gep16, align 4
  %3 = icmp slt i32 %arr.load, 20
  %..i = select i1 %3, i32 %arr.load, i32 20
  %4 = icmp sgt i32 %..i, 1
  %common.ret.op.i = select i1 %4, i32 %..i, i32 1
  %add = add i32 %common.ret.op.i, %total.033
  %inc18 = add nuw nsw i32 %i.132, 1
  %lt13 = icmp slt i32 %inc18, %0
  br i1 %lt13, label %for.body8, label %for.end10

for.end10:                                        ; preds = %for.body8, %entry, %for.cond7.preheader
  %total.0.lcssa = phi i32 [ 0, %for.cond7.preheader ], [ 0, %entry ], [ %add, %for.body8 ]
  ret i32 %total.0.lcssa
}

; Function Attrs: nofree nosync nounwind readnone
define i32 @main() local_unnamed_addr #2 {
ifcont.4:
  ret i32 58
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
attributes #1 = { nofree norecurse nosync nounwind readnone }
attributes #2 = { nofree nosync nounwind readnone }
