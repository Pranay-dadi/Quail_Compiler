; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @clamp(i32 %0, i32 %1, i32 %2) local_unnamed_addr #0 {
entry:
  %3 = icmp slt i32 %0, %1
  %4 = icmp sgt i32 %0, %2
  %. = select i1 %4, i32 %2, i32 %0
  %common.ret.op = select i1 %3, i32 %1, i32 %.
  ret i32 %common.ret.op
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @sumArray(i32 %0) local_unnamed_addr #1 {
entry:
  %arr = alloca [10 x i32], align 4
  %1 = icmp sgt i32 %0, 0
  br i1 %1, label %for.body, label %for.end9

for.cond6.preheader:                              ; preds = %for.body
  br i1 %1, label %for.body7, label %for.end9

for.body:                                         ; preds = %entry, %for.body
  %i.025 = phi i32 [ %4, %for.body ], [ 0, %entry ]
  %2 = mul i32 %i.025, %i.025
  %3 = zext i32 %i.025 to i64
  %arr.gep = getelementptr [10 x i32], [10 x i32]* %arr, i64 0, i64 %3
  store i32 %2, i32* %arr.gep, align 4
  %4 = add nuw nsw i32 %i.025, 1
  %5 = icmp slt i32 %4, %0
  br i1 %5, label %for.body, label %for.cond6.preheader

for.body7:                                        ; preds = %for.cond6.preheader, %for.body7
  %total.027 = phi i32 [ %9, %for.body7 ], [ 0, %for.cond6.preheader ]
  %i.126 = phi i32 [ %10, %for.body7 ], [ 0, %for.cond6.preheader ]
  %6 = zext i32 %i.126 to i64
  %arr.gep14 = getelementptr [10 x i32], [10 x i32]* %arr, i64 0, i64 %6
  %arr.load = load i32, i32* %arr.gep14, align 4
  %7 = icmp slt i32 %arr.load, 20
  %..i = select i1 %7, i32 %arr.load, i32 20
  %8 = icmp sgt i32 %..i, 1
  %common.ret.op.i = select i1 %8, i32 %..i, i32 1
  %9 = add i32 %common.ret.op.i, %total.027
  %10 = add nuw nsw i32 %i.126, 1
  %11 = icmp slt i32 %10, %0
  br i1 %11, label %for.body7, label %for.end9

for.end9:                                         ; preds = %for.body7, %entry, %for.cond6.preheader
  %total.0.lcssa = phi i32 [ 0, %for.cond6.preheader ], [ 0, %entry ], [ %9, %for.body7 ]
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
