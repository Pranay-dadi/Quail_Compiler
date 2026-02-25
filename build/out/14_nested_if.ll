; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @grade(i32 %0) local_unnamed_addr #0 {
entry:
  %1 = icmp sgt i32 %0, 89
  br i1 %1, label %common.ret, label %else

common.ret:                                       ; preds = %else4, %else, %entry
  %common.ret.op = phi i32 [ 4, %entry ], [ 3, %else ], [ %., %else4 ]
  ret i32 %common.ret.op

else:                                             ; preds = %entry
  %2 = icmp sgt i32 %0, 74
  br i1 %2, label %common.ret, label %else4

else4:                                            ; preds = %else
  %3 = icmp sgt i32 %0, 59
  %. = select i1 %3, i32 2, i32 1
  br label %common.ret
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #0 {
entry:
  ret i32 3
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
