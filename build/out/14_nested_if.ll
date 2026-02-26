; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @grade(i32 %0) local_unnamed_addr #0 {
entry:
  %ge = icmp sgt i32 %0, 89
  br i1 %ge, label %common.ret, label %else

common.ret:                                       ; preds = %else5, %else, %entry
  %common.ret.op = phi i32 [ 4, %entry ], [ 3, %else ], [ %., %else5 ]
  ret i32 %common.ret.op

else:                                             ; preds = %entry
  %ge3 = icmp sgt i32 %0, 74
  br i1 %ge3, label %common.ret, label %else5

else5:                                            ; preds = %else
  %ge8 = icmp sgt i32 %0, 59
  %. = select i1 %ge8, i32 2, i32 1
  br label %common.ret
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #0 {
entry:
  ret i32 3
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
